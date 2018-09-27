// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/proposer.h>

#include <address/address.h>
#include <chainparams.h>
#include <esperanza/kernel.h>
#include <esperanza/stakevalidation.h>
#include <net.h>
#include <script/script.h>
#include <timedata.h>
#include <util.h>
#include <utilmoneystr.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <atomic>
#include <chrono>
#include <memory>

namespace esperanza {

Proposer::Thread::Thread(const std::string &threadName, const Settings &config,
                         const std::vector<CWallet *> &wallets,
                         CountingSemaphore &initSemaphore,
                         CountingSemaphore &startSemaphore,
                         CountingSemaphore &stopSemaphore)
    : m_threadName(threadName),
      m_settings(config),
      m_interrupted(false),
      m_waiter(),
      m_wallets(wallets),
      m_initSemaphore(initSemaphore),
      m_startSemaphore(startSemaphore),
      m_stopSemaphore(stopSemaphore) {
  std::thread thread(Proposer::Run, std::ref(*this));
  thread.detach();
}

void Proposer::Thread::Stop() {
  m_interrupted = true;
  Wake();
}

void Proposer::Thread::Wake() { m_waiter.Wake(); }

void Proposer::Thread::SetStatus(const Proposer::Status status,
                                 CWallet *const wallet) {
  if (wallet) {
    wallet->GetWalletExtension().m_proposerState.m_status = status;
  } else {
    for (CWallet *w : m_wallets) {
      SetStatus(status, w);
    }
  }
}

std::vector<std::unique_ptr<Proposer::Thread>> Proposer::CreateProposerThreads(
    const Settings &settings, const std::vector<CWallet *> &wallets,
    CountingSemaphore &initSemaphore, CountingSemaphore &startSemaphore,
    CountingSemaphore &stopSemaphore) {
  // total number of threads can not exceed number of wallets
  const size_t numThreads = std::min(
      wallets.size(), std::max<size_t>(1, settings.m_numberOfProposerThreads));

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
    const std::string threadName =
        settings.m_proposerThreadName + "-" + std::to_string(threadIx);
    threads.emplace_back(std::unique_ptr<Proposer::Thread>(
        new Proposer::Thread(threadName, settings, thisThreadsWallets,
                             initSemaphore, startSemaphore, stopSemaphore)));
  }

  initSemaphore.acquire(numThreads);
  LogPrint(BCLog::ESPERANZA, "%d proposer threads initialized.\n", numThreads);

  return threads;
}

Proposer::Proposer(const Settings &settings,
                   const std::vector<CWallet *> &wallets)
    : m_initSemaphore(0),
      m_startSemaphore(0),
      m_stopSemaphore(0),
      m_threads(CreateProposerThreads(settings, wallets, m_initSemaphore,
                                      m_startSemaphore, m_stopSemaphore)) {}

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
  LogPrint(BCLog::ESPERANZA, "%s: initialized.\n", thread.m_threadName.c_str());
  for (const auto wallet : thread.m_wallets) {
    LogPrint(BCLog::ESPERANZA, "  responsible for: %s\n", wallet->GetName());
  }
  thread.m_initSemaphore.release();
  thread.m_startSemaphore.acquire();
  LogPrint(BCLog::ESPERANZA, "%s: started.\n", thread.m_threadName.c_str());

  while (!thread.m_interrupted) {
    try {
      const auto blockDownloadStatus = GetInitialBlockDownloadStatus();
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
        LOCK(cs_main);
        bestHeight = chainActive.Height();
        bestTime = chainActive.Tip()->nTime;
      }

      // UNIT-E: respect thread.m_config.m_minProposeInterval

      const int64_t currentTime = GetAdjustedTime();
      const int64_t mask = ::Params().GetEsperanza().GetStakeTimestampMask();
      const int64_t searchTime = currentTime & ~mask;

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
      auto sleepFor = thread.m_settings.m_proposerSleep;
      const auto setSleepDuration = [&sleepFor](const decltype(sleepFor) amount) {
        sleepFor = std::min(sleepFor, amount);
      };
      for (CWallet *wallet : thread.m_wallets) {
        auto &walletExt = wallet->GetWalletExtension();

        const int64_t waitTill =
            walletExt.m_proposerState.m_lastTimeProposed +
            seconds(thread.m_settings.m_minProposeInterval);
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

        CScript coinbaseScript;
        std::unique_ptr<CBlockTemplate> blockTemplate =
            BlockAssembler(::Params())
                .CreateNewBlock(coinbaseScript, /* fMineWitnessTx */ true);

        if (!blockTemplate) {
          LogPrint(BCLog::ESPERANZA, "%s/%s: failed to get block template",
                   thread.m_threadName, wallet->GetName());
          continue;
        }

        if (walletExt.SignBlock(blockTemplate.get(), bestHeight + 1,
                                searchTime)) {
          const CBlock &block = blockTemplate->block;
          if (!ProposeBlock(blockTemplate->block)) {
            LogPrint(BCLog::ESPERANZA, "%s/%s: failed to propose block",
                     thread.m_threadName, wallet->GetName());
            continue;
          }
          walletExt.m_proposerState.m_lastTimeProposed = block.nTime;
          // we got lucky and proposed, enough for this round (other wallets
          // need not be checked no more)
          break;
        }
      }
      thread.Sleep(sleepFor);
    } catch (const std::runtime_error &error) {
      // this log statement does not mention a category as it captches
      // exceptions that are not supposed to happen
      LogPrint(BCLog::ESPERANZA, "%s: exception in proposer thread: %s\n",
               thread.m_threadName, error.what());
    } catch (...) {
      // this log statement does not mention a category as it captches
      // exceptions that are not supposed to happen
      LogPrint(BCLog::ESPERANZA, "%s: unknown exception in proposer thread.\n",
               thread.m_threadName);
    }
  }
  LogPrint(BCLog::ESPERANZA, "%s: stopping...\n", thread.m_threadName.c_str());
  thread.m_stopSemaphore.release();
}

}  // namespace esperanza
