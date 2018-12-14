// Copyright (c) 2018 The Unit-e developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer.h>

#include <chainparams.h>
#include <net.h>
#include <script/script.h>
#include <settings.h>
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
    wallet->GetWalletExtension().GetProposerState().m_status = status;
  } else {
    for (CWallet *w : m_wallets) {
      SetStatus(status, w);
    }
  }
}

void ProposerImpl::CreateProposerThreads() {
  const std::vector<CWallet *> &wallets = m_multi_wallet->GetWallets();
  // total number of threads can not exceed number of wallets
  const size_t numThreads = 1;

  using WalletIndex = size_t;
  using ThreadIndex = size_t;

  // mapping of which thread is responsible for which wallet
  std::multimap<ThreadIndex, WalletIndex> indexMap;

  // distribute wallets across threads
  for (WalletIndex walletIx = 0; walletIx < wallets.size(); ++walletIx) {
    indexMap.insert({walletIx % numThreads, walletIx});
  }

  m_threads.reinitialize(numThreads);

  // create thread objects
  for (ThreadIndex threadIx = 0; threadIx < numThreads; ++threadIx) {
    std::vector<CWallet *> thisThreadsWallets;
    const auto walletRange = indexMap.equal_range(threadIx);
    for (auto entry = walletRange.first; entry != walletRange.second; ++entry) {
      thisThreadsWallets.push_back(wallets[entry->second]);
    }
    std::string threadName = "proposer-" + std::to_string(threadIx);
    m_threads.emplace_back(threadName, *this, std::move(thisThreadsWallets));
  }

  m_init_semaphore.acquire(numThreads);
  LogPrint(BCLog::PROPOSING, "%d proposer threads initialized.\n", numThreads);
}

ProposerImpl::ProposerImpl(Dependency<Settings> settings,
                           Dependency<MultiWallet> multiWallet,
                           Dependency<staking::Network> networkInterface,
                           Dependency<staking::ActiveChain> chainInterface)
    : m_settings(settings),
      m_multi_wallet(multiWallet),
      m_network(networkInterface),
      m_chain(chainInterface),
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
        wallet->GetWalletExtension().GetProposerState().m_number_of_search_attempts += 1;
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

      for (CWallet *wallet : thread.m_wallets) {
        auto &wallet_ext = wallet->GetWalletExtension();

        if (wallet->IsLocked()) {
          thread.SetStatus(Status::NOT_PROPOSING_WALLET_LOCKED, wallet);
          continue;
        }

        {
          LOCK2(thread.m_proposer.m_chain->GetLock(), wallet_ext.GetLock());

          if (wallet_ext.GetStakeableBalance() <= wallet_ext.GetReserveBalance()) {
            thread.SetStatus(Status::NOT_PROPOSING_NOT_ENOUGH_BALANCE, wallet);
            continue;
          }

          thread.SetStatus(Status::IS_PROPOSING, wallet);

          wallet_ext.GetProposerState().m_number_of_searches += 1;

          CScript coinbaseScript;
          std::unique_ptr<CBlockTemplate> blockTemplate =
              BlockAssembler(::Params())
                  .CreateNewBlock(coinbaseScript, /* fMineWitnessTx */ true);

          if (!blockTemplate) {
            LogPrint(BCLog::PROPOSING, "%s/%s: failed to get block template",
                     thread.m_thread_name, wallet->GetName());
            continue;
          }

          std::shared_ptr<const CBlock> block;

          if (block) {
            wallet_ext.GetProposerState().m_last_time_proposed = block->nTime;
            // we got lucky and proposed, enough for this round (other wallets
            // need not be checked no more)
            break;
          } else {
            // failed to propose block
            LogPrint(BCLog::PROPOSING, "failed to propose block.\n");
            continue;
          }
        }
      }
      thread.Sleep(std::chrono::seconds(15));
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
    Dependency<MultiWallet> multi_wallet,
    Dependency<staking::Network> network,
    Dependency<staking::ActiveChain> active_chain) {
  if (settings->node_is_proposer) {
    return std::unique_ptr<Proposer>(new ProposerImpl(settings, multi_wallet, network, active_chain));
  } else {
    return std::unique_ptr<Proposer>(new ProposerStub());
  }
}

}  // namespace proposer
