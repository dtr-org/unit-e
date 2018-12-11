// Copyright (c) 2018 The Unit-e developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer.h>

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

ProposerImpl::Thread::Thread(const std::string &threadName,
                             ProposerImpl &parentProposer,
                             std::vector<CWallet *> &&wallets)
    : m_thread_name(threadName),
      m_proposer(parentProposer),
      m_interrupted(false),
      m_waiter(),
      m_wallets(std::move(wallets)),
      m_thread(ProposerImpl::Run, std::ref(*this)) {}

void ProposerImpl::Thread::Stop() {
  LogPrint(BCLog::PROPOSING, "Stopping proposer-thread %s...\n", m_thread_name);
  m_interrupted = true;
  Wake();
}

void ProposerImpl::Thread::Join() {
  m_thread.join();
  LogPrint(BCLog::PROPOSING, "Stopped proposer-thread %s\n", m_thread_name);
}

void ProposerImpl::Thread::Wake() {
  LogPrint(BCLog::PROPOSING, "Waking proposer-thread \"%s\"\n", m_thread_name);
  m_waiter.Wake();
}

void ProposerImpl::Thread::SetStatus(const Status status, CWallet *const wallet) {
  if (wallet) {
    wallet->GetWalletExtension().m_proposer_state.m_status = status;
  } else {
    for (CWallet *w : m_wallets) {
      SetStatus(status, w);
    }
  }
}

void ProposerImpl::CreateProposerThreads() {
  const std::vector<CWallet *> &wallets = m_multi_wallet->GetWallets();
  // total number of threads can not exceed number of wallets
  const size_t numThreads =
      std::min(wallets.size(),
               std::max<size_t>(1, m_settings->number_of_proposer_threads));

  using WalletIndex = size_t;
  using ThreadIndex = size_t;

  // mapping of which thread is responsible for which wallet
  std::multimap<ThreadIndex, WalletIndex> indexMap;

  // distribute wallets across threads
  for (WalletIndex walletIx = 0; walletIx < wallets.size(); ++walletIx) {
    indexMap.insert({walletIx % numThreads, walletIx});
  }

  m_threads.resize(numThreads);

  // create thread objects
  for (ThreadIndex threadIx = 0; threadIx < numThreads; ++threadIx) {
    std::vector<CWallet *> thisThreadsWallets;
    const auto walletRange = indexMap.equal_range(threadIx);
    for (auto entry = walletRange.first; entry != walletRange.second; ++entry) {
      thisThreadsWallets.push_back(wallets[entry->second]);
    }
    std::string threadName =
        m_settings->proposer_thread_prefix + "-" + std::to_string(threadIx);
    m_threads.emplace_back(threadName, *this, std::move(thisThreadsWallets));
  }

  m_init_semaphore.acquire(numThreads);
  LogPrint(BCLog::PROPOSING, "%d proposer threads initialized.\n", numThreads);
}

ProposerImpl::ProposerImpl(Dependency<Settings> settings,
                           Dependency<MultiWallet> multiWallet,
                           Dependency<staking::Network> networkInterface,
                           Dependency<staking::ActiveChain> chainInterface,
                           Dependency<BlockProposer> blockProposer)
    : m_settings(settings),
      m_multi_wallet(multiWallet),
      m_network(networkInterface),
      m_chain(chainInterface),
      m_block_proposer(blockProposer),
      m_init_semaphore(0),
      m_stop_semaphore(0),
      m_threads() {}

ProposerImpl::~ProposerImpl() {
  if (!m_started.test_and_set()) {
    LogPrint(BCLog::PROPOSING, "Freeing proposer (was not started)...\n");
    return;
  }
  LogPrint(BCLog::PROPOSING, "Stopping proposer...\n");
  for (auto &thread : m_threads) {
    thread.Stop();
  }
  for (auto &thread : m_threads) {
    thread.Join();
  }
}

void ProposerImpl::Start() {
  if (m_started.test_and_set()) {
    LogPrint(BCLog::PROPOSING, "WARN: Proposer started twice.\n");
    return;
  }
  LogPrint(BCLog::PROPOSING, "Creating proposer threads...\n");
  CreateProposerThreads();

  LogPrint(BCLog::PROPOSING, "Starting proposer.\n");
  m_stop_semaphore.release(m_threads.size());
}

void ProposerImpl::Wake(const CWallet *wallet) {
  if (wallet) {
    // find and wake the thread that is responsible for this wallet
    for (auto &thread : m_threads) {
      for (const auto w : thread.m_wallets) {
        if (w == wallet) {
          thread.Wake();
          return;
        }
      }
    }
    // wake all threads
  } else {
    for (auto &thread : m_threads) {
      thread.Wake();
    }
  }
}

template <typename Duration>
int64_t seconds(const Duration t) {
  return std::chrono::duration_cast<std::chrono::seconds>(t).count();
}

void ProposerImpl::Run(ProposerImpl::Thread &thread) {
  RenameThread(thread.m_thread_name.c_str());
  LogPrint(BCLog::PROPOSING, "%s: initialized.\n", thread.m_thread_name.c_str());
  thread.m_proposer.m_init_semaphore.release();
  thread.m_proposer.m_stop_semaphore.acquire();
  LogPrint(BCLog::PROPOSING, "%s: started.\n", thread.m_thread_name.c_str());

  while (!thread.m_interrupted) {
    try {
      for (auto *wallet : thread.m_wallets) {
        wallet->GetWalletExtension().m_proposer_state.m_number_of_search_attempts += 1;
      }
      const auto blockDownloadStatus =
          thread.m_proposer.m_chain->GetInitialBlockDownloadStatus();
      if (blockDownloadStatus != +SyncStatus::SYNCED) {
        thread.SetStatus(Status::NOT_PROPOSING_SYNCING_BLOCKCHAIN);
        thread.Sleep(std::chrono::seconds(30));
        continue;
      }
      if (thread.m_proposer.m_network->GetNodeCount() == 0) {
        thread.SetStatus(Status::NOT_PROPOSING_NO_PEERS);
        thread.Sleep(std::chrono::seconds(30));
        continue;
      }

      uint32_t bestHeight;
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
            seconds(thread.m_proposer.m_settings->min_propose_interval);
        const int64_t lastTimeProposed =
            wallet->GetWalletExtension().m_proposer_state.m_last_time_proposed;
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
      auto sleepFor = thread.m_proposer.m_settings->proposer_sleep;
      const auto setSleepDuration =
          [&sleepFor](const decltype(sleepFor) amount) {
            sleepFor = std::min(sleepFor, amount);
          };
      for (CWallet *wallet : thread.m_wallets) {
        auto &walletExt = wallet->GetWalletExtension();

        const int64_t waitTill =
            walletExt.m_proposer_state.m_last_time_proposed +
            seconds(thread.m_proposer.m_settings->min_propose_interval);
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
        if (walletExt.GetStakeableBalance() <= walletExt.m_reserve_balance) {
          thread.SetStatus(Status::NOT_PROPOSING_NOT_ENOUGH_BALANCE, wallet);
          continue;
        }

        thread.SetStatus(Status::IS_PROPOSING, wallet);

        walletExt.m_proposer_state.m_number_of_searches += 1;

        CScript coinbaseScript;
        std::unique_ptr<CBlockTemplate> blockTemplate =
            BlockAssembler(::Params())
                .CreateNewBlock(coinbaseScript, /* fMineWitnessTx */ true);

        if (!blockTemplate) {
          LogPrint(BCLog::PROPOSING, "%s/%s: failed to get block template",
                   thread.m_thread_name, wallet->GetName());
          continue;
        }

        BlockProposer::ProposeBlockParameters blockProposal{};
        blockProposal.wallet = &walletExt;
        blockProposal.blockHeight = bestHeight;
        blockProposal.blockTime = bestTime;

        std::shared_ptr<const CBlock> block =
            thread.m_proposer.m_block_proposer->ProposeBlock(blockProposal);

        if (block) {
          walletExt.m_proposer_state.m_last_time_proposed = block->nTime;
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
               thread.m_thread_name, error.what());
    } catch (...) {
      // this log statement does not mention a category as it captches
      // exceptions that are not supposed to happen
      LogPrint(BCLog::PROPOSING, "%s: unknown exception in proposer thread.\n",
               thread.m_thread_name);
    }
  }
}

std::unique_ptr<Proposer> Proposer::New(
    Dependency<Settings> settings,
    Dependency<MultiWallet> multiWallet,
    Dependency<staking::Network> network,
    Dependency<staking::ActiveChain> chainState,
    Dependency<BlockProposer> blockProposer) {
  if (settings->proposing) {
    return std::unique_ptr<Proposer>(new ProposerImpl(settings, multiWallet, network, chainState, blockProposer));
  } else {
    return std::unique_ptr<Proposer>(new ProposerStub());
  }
}

}  // namespace proposer
