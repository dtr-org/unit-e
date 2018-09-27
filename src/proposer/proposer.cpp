// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer.h>

#include <address/address.h>
#include <chainparams.h>
#include <net.h>
#include <script/script.h>
#include <sync.h>
#include <util.h>
#include <utilmoneystr.h>
#include <wallet/wallet.h>

#include <atomic>
#include <chrono>
#include <memory>

namespace proposer {

Proposer::Thread::Thread(const std::string &threadName,
                         Proposer &parentProposer,
                         const std::vector<CWallet *> &wallets)
    : m_threadName(threadName),
      m_proposer(parentProposer),
      m_interrupted(false),
      m_waiter(),
      m_wallets(wallets) {
  std::thread thread(Proposer::Run, std::ref(*this));
  thread.detach();
}

void Proposer::Thread::Stop() {
  m_interrupted = true;
  Wake();
}

void Proposer::Thread::Wake() { m_waiter.Wake(); }

void Proposer::Thread::SetStatus(const Status status, CWallet *const wallet) {
  if (wallet) {
    wallet->GetWalletExtension().m_proposerState.m_status = status;
  } else {
    for (CWallet *w : m_wallets) {
      SetStatus(status, w);
    }
  }
}

std::vector<std::unique_ptr<Proposer::Thread>> Proposer::CreateProposerThreads(
    const std::vector<CWallet *> &wallets) {
  // total number of threads can not exceed number of wallets
  const size_t numThreads =
      std::min(wallets.size(),
               std::max<size_t>(1, m_settings.m_numberOfProposerThreads));

  using WalletIndex = size_t;
  using ThreadIndex = size_t;

  // mapping of which thread is responsible for which wallet
  std::multimap<ThreadIndex, WalletIndex> indexMap;

  // distribute wallets across threads
  for (WalletIndex walletIx = 0; walletIx < wallets.size(); ++walletIx) {
    indexMap.insert({walletIx % numThreads, walletIx});
  }

  // create thread objects
  std::vector<std::unique_ptr<Proposer::Thread>> threads;
  for (ThreadIndex threadIx = 0; threadIx < numThreads; ++threadIx) {
    std::vector<CWallet *> thisThreadsWallets;
    const auto walletRange = indexMap.equal_range(threadIx);
    for (auto entry = walletRange.first; entry != walletRange.second; ++entry) {
      thisThreadsWallets.push_back(wallets[entry->second]);
    }
    std::string threadName =
        m_settings.m_proposerThreadName + "-" + std::to_string(threadIx);
    threads.emplace_back(std::unique_ptr<Proposer::Thread>(
        new Proposer::Thread(threadName, *this, thisThreadsWallets)));
  }

  m_initSemaphore.acquire(numThreads);
  LogPrint(BCLog::PROPOSING, "%d proposer threads initialized.\n", numThreads);

  return threads;
}

Proposer::Proposer(const esperanza::Settings &settings,
                   const std::vector<CWallet *> &wallets,
                   Dependency<Network> networkInterface,
                   Dependency<ChainState> chainInterface,
                   Dependency<BlockProposer> blockProposer)
    : m_settings(settings),
      m_network(networkInterface),
      m_chain(chainInterface),
      m_blockProposer(blockProposer),
      m_initSemaphore(0),
      m_startSemaphore(0),
      m_stopSemaphore(0),
      m_threads(CreateProposerThreads(wallets)) {}

Proposer::~Proposer() { Stop(); }

void Proposer::Start() { m_startSemaphore.release(m_threads.size()); }

void Proposer::Stop() {
  for (const auto &thread : m_threads) {
    // sets all threads m_interrupted and wakes them up in case they are
    // sleeping
    thread->Stop();
  }
  // in case Start() was not called yet, start the threads so they can stop
  // (otherwise they are stuck)
  m_startSemaphore.release(m_threads.size());
  // wait for the threads to finish (important for the destructor, otherwise
  // memory might be released which is still accessed by a thread)
  m_stopSemaphore.acquire(m_threads.size());
  // in case Stop() is going to be invoked twice (important for destructor) make
  // sure there are enough permits in the stop semaphore for another invocation
  m_stopSemaphore.release(m_threads.size());
}

void Proposer::Wake(const CWallet *wallet) {
  if (wallet) {
    // find and wake the thread that is responsible for this wallet
    for (const auto &thread : m_threads) {
      for (const auto w : thread->m_wallets) {
        if (w == wallet) {
          thread->Wake();
          return;
        }
      }
    }
    // wake all threads
  } else {
    for (const auto &thread : m_threads) {
      thread->Wake();
    }
  }
}

template <typename Duration>
int64_t seconds(const Duration t) {
  return std::chrono::duration_cast<std::chrono::seconds>(t).count();
}

void Proposer::Run(Proposer::Thread &thread) {
  LogPrint(BCLog::PROPOSING, "%s: initialized.\n", thread.m_threadName.c_str());
  for (const auto wallet : thread.m_wallets) {
    LogPrint(BCLog::PROPOSING, "  responsible for: %s\n", wallet->GetName());
  }
  thread.m_proposer.m_initSemaphore.release();
  thread.m_proposer.m_startSemaphore.acquire();
  LogPrint(BCLog::PROPOSING, "%s: started.\n", thread.m_threadName.c_str());

  while (!thread.m_interrupted) {
    try {
      for (auto *wallet : thread.m_wallets) {
        wallet->GetWalletExtension().m_proposerState.m_numSearchAttempts += 1;
      }
      const auto blockDownloadStatus =
          thread.m_proposer.m_chain->GetInitialBlockDownloadStatus();
      if (blockDownloadStatus != +SyncStatus::SYNCED) {
        thread.SetStatus(Status::NOT_PROPOSING_SYNCING_BLOCKCHAIN);
        thread.Sleep(std::chrono::seconds(30));
        continue;
      }
      if (g_connman->GetNodeCount() == 0) {
        thread.SetStatus(Status::NOT_PROPOSING_NO_PEERS);
        thread.Sleep(std::chrono::seconds(30));
        continue;
      }

      int bestHeight;
      int64_t bestTime;
      {
        LOCK(thread.m_proposer.m_chain->GetLock());
        bestHeight = thread.m_proposer.m_chain->GetHeight();
        bestTime = thread.m_proposer.m_chain->GetTip()->nTime;
      }

      const int64_t currentTime = thread.m_proposer.m_network->GetTime();
      const int64_t mask = ::Params().GetEsperanza().GetStakeTimestampMask();
      const int64_t searchTime = currentTime & ~mask;

      for (auto *wallet : thread.m_wallets) {
        const int64_t gracePeriod =
            seconds(thread.m_proposer.m_settings.m_minProposeInterval);
        const int64_t lastTimeProposed =
            wallet->GetWalletExtension().m_proposerState.m_lastTimeProposed;
        const int64_t timeSinceLastProposal = currentTime - lastTimeProposed;
        const int64_t gracePeriodRemaining =
            gracePeriod - timeSinceLastProposal;
        if (gracePeriodRemaining > 0) {
          thread.SetStatus(Status::JUST_PROPOSED_GRACE_PERIOD);
          thread.Sleep(std::chrono::seconds(gracePeriodRemaining));
          continue;
        }
      }

      if (searchTime < bestTime) {
        if (currentTime < bestTime) {
          // lagging behind - can't propose before most recent block
          std::chrono::seconds lag =
              std::chrono::seconds(bestTime - currentTime);
          thread.Sleep(lag);
        } else {
          // due to timestamp mask time was truncated to a point before best
          // block time
          const int64_t nextSearch = searchTime + mask;
          std::chrono::seconds timeTillNextSearch =
              std::chrono::seconds(nextSearch - currentTime);
          thread.Sleep(timeTillNextSearch);
        }
        continue;
      }

      // each wallet may be blocked from proposing for a different reason
      // and induce a sleep for a different duration. The thread as a whole
      // only has to sleep as long as the minimum of these durations to check
      // the wallet which is due next in time.
      auto sleepFor = thread.m_proposer.m_settings.m_proposerSleep;
      const auto setSleepDuration =
          [&sleepFor](const decltype(sleepFor) amount) {
            sleepFor = std::min(sleepFor, amount);
          };
      for (CWallet *wallet : thread.m_wallets) {
        auto &walletExt = wallet->GetWalletExtension();

        const int64_t waitTill =
            walletExt.m_proposerState.m_lastTimeProposed +
            seconds(thread.m_proposer.m_settings.m_minProposeInterval);
        if (bestTime < waitTill) {
          const decltype(sleepFor) amount =
              std::chrono::seconds(waitTill - bestTime);
          setSleepDuration(amount);
          continue;
        }
        if (wallet->IsLocked()) {
          thread.SetStatus(Status::NOT_PROPOSING_WALLET_LOCKED, wallet);
          continue;
        }
        if (walletExt.GetStakeableBalance() <= walletExt.m_reserveBalance) {
          thread.SetStatus(Status::NOT_PROPOSING_NOT_ENOUGH_BALANCE, wallet);
          continue;
        }

        thread.SetStatus(Status::IS_PROPOSING, wallet);

        walletExt.m_proposerState.m_numSearches += 1;

        CScript coinbaseScript;
        std::unique_ptr<CBlockTemplate> blockTemplate =
            BlockAssembler(::Params())
                .CreateNewBlock(coinbaseScript, /* fMineWitnessTx */ true);

        if (!blockTemplate) {
          LogPrint(BCLog::PROPOSING, "%s/%s: failed to get block template",
                   thread.m_threadName, wallet->GetName());
          continue;
        }

        BlockProposer::ProposeBlockParameters blockProposal{};
        blockProposal.wallet = &walletExt;
        blockProposal.blockHeight = bestHeight;
        blockProposal.blockTime = bestTime;

        std::shared_ptr<const CBlock> block =
            thread.m_proposer.m_blockProposer->ProposeBlock(blockProposal);

        if (block) {
          walletExt.m_proposerState.m_lastTimeProposed = block->nTime;
          // we got lucky and proposed, enough for this round (other wallets
          // need not be checked no more)
          break;
        } else {
          // failed to propose block
          LogPrint(BCLog::PROPOSING, "failed to propose block.\n");
          continue;
        }
      }
      thread.Sleep(sleepFor);
    } catch (const std::runtime_error &error) {
      // this log statement does not mention a category as it captches
      // exceptions that are not supposed to happen
      LogPrint(BCLog::PROPOSING, "%s: exception in proposer thread: %s\n",
               thread.m_threadName, error.what());
    } catch (...) {
      // this log statement does not mention a category as it captches
      // exceptions that are not supposed to happen
      LogPrint(BCLog::PROPOSING, "%s: unknown exception in proposer thread.\n",
               thread.m_threadName);
    }
  }
  LogPrint(BCLog::PROPOSING, "%s: stopping...\n", thread.m_threadName);
  thread.m_proposer.m_stopSemaphore.release();
}

}  // namespace proposer
