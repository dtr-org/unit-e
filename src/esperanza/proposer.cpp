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
#include "proposer.h"

namespace esperanza {

Proposer::Thread::Thread(const std::string &threadName, const Config &config,
                         const std::vector<CWallet *> &wallets,
                         Semaphore &initSemaphore, Semaphore &startSemaphore,
                         Semaphore &stopSemaphore)
    : m_threadName(threadName),
      m_config(config),
      m_interrupted(false),
      m_waiter(),
      m_wallets(wallets),
      m_initSemaphore(initSemaphore),
      m_startSemaphore(startSemaphore),
      m_stopSemaphore(stopSemaphore),
      m_thread(std::thread{Proposer::Run, std::ref(*this)}) {}

void Proposer::Thread::Stop() {
  m_thread.detach();
  m_interrupted = true;
  Wake();
}

void Proposer::Thread::Wake() { m_waiter.WakeOne(); }

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
    const Config &config, const std::vector<CWallet *> &wallets,
    Semaphore &initSemaphore, Semaphore &startSemaphore,
    Semaphore &stopSemaphore) {
  // total number of threads can not exceed number of wallets
  const size_t numThreads = std::min(
      wallets.size(), std::max<size_t>(1, config.m_numberOfProposerThreads));

  LogPrintf("creating %d proposer threads (%d requested, %d wallets)...\n",
            numThreads, config.m_numberOfProposerThreads, wallets.size());

  using WalletIndex = size_t;
  using ThreadIndex = size_t;

  // mapping of which thread is responsible for which wallet
  std::multimap<ThreadIndex, WalletIndex> indexMap;

  // distribute wallets across threads
  for (WalletIndex walletIx = 0; walletIx < numThreads; ++walletIx) {
    indexMap.insert({walletIx % numThreads, walletIx});
  }

  // create thread objects
  std::vector<std::unique_ptr<Proposer::Thread>> threads;
  for (ThreadIndex threadIx = 0; threadIx < numThreads; ++threadIx) {
    std::vector<CWallet *> thisThreadsWallets;
    auto walletRange = indexMap.equal_range(threadIx);
    for (auto entry = walletRange.first; entry != walletRange.second; ++entry) {
      thisThreadsWallets.push_back(wallets[entry->second]);
    }
    const std::string threadName =
        config.m_proposerThreadName + "-" + std::to_string(threadIx);
    threads.emplace_back(std::unique_ptr<Proposer::Thread>(
        new Proposer::Thread(threadName, config, thisThreadsWallets,
                             initSemaphore, startSemaphore, stopSemaphore)));
  }

  initSemaphore.acquire(numThreads);
  LogPrintf("%d proposer threads initialized.\n", numThreads);

  return threads;
}

Proposer::Proposer(const Config &config, const std::vector<CWallet *> &wallets)
    : m_initSemaphore(0),
      m_startSemaphore(0),
      m_stopSemaphore(0),
      m_threads(CreateProposerThreads(config, wallets, m_initSemaphore,
                                      m_startSemaphore, m_stopSemaphore)) {}

void Proposer::Start() {
  LogPrintf("starting %d proposer threads...\n", m_threads.size());
  m_startSemaphore.release(m_threads.size());
}

void Proposer::Stop() {
  LogPrint(BCLog::ESPERANZA, "stopping %d proposer threads...\n",
           m_threads.size());
  for (const auto &thread : m_threads) {
    thread->Stop();
  }
  m_stopSemaphore.acquire(m_threads.size());
  LogPrint(BCLog::ESPERANZA, "all proposer threads exited.\n");
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
      if (fReindex) {
        thread.SetStatus(Status::NOT_PROPOSING_REINDEXING);
        continue;
      }
      if (fImporting) {
        thread.SetStatus(Status::NOT_PROPOSING_IMPORTING);
        continue;
      }
      if (IsInitialBlockDownload()) {
        thread.SetStatus(Status::NOT_PROPOSING_SYNCING_BLOCKCHAIN);
        continue;
      }
      if (g_connman->GetNodeCount() == 0) {
        thread.SetStatus(Status::NOT_PROPOSING_NO_PEERS);
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

      int64_t currentTime = GetAdjustedTime();
      int64_t mask = ::Params().EsperanzaParams().GetStakeTimestampMask();
      int64_t searchTime = currentTime & ~mask;

      if (searchTime < bestTime) {
        if (currentTime < bestTime) {
          // lagging behind - can't propose before most recent block
          std::chrono::seconds lag =
              std::chrono::seconds(bestTime - currentTime);
          thread.Sleep(lag);
        } else {
          // due to timestamp mask time was truncated to a point before best
          // block time
          int64_t nextSearch = searchTime + mask;
          std::chrono::seconds timeTillNextSearch =
              std::chrono::seconds(nextSearch - currentTime);
          thread.Sleep(timeTillNextSearch);
        }
        continue;
      }

      for (CWallet *wallet : thread.m_wallets) {
        auto &walletExt = wallet->GetWalletExtension();

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
          LogPrint(BCLog::ESPERANZA, "failed to get block template in %s",
                   __func__);
          continue;
        }

        if (walletExt.SignBlock(blockTemplate.get(), bestHeight + 1,
                                searchTime)) {
          if (!ProposeBlock(blockTemplate->block)) {
            continue;
          }
          // set last proposing time
          break;
        }
      }

      thread.m_waiter.WaitUpTo(
          std::chrono::seconds(30));  // stub for testing for now
    } catch (const std::runtime_error &error) {
      LogPrint(BCLog::ESPERANZA, "exception in proposer thread: %s\n",
               error.what());
    } catch (...) {
      LogPrint(BCLog::ESPERANZA, "unknown exception in proposer thread.\n");
    }
  }
  LogPrint(BCLog::ESPERANZA, "%s: stopping...\n", thread.m_threadName.c_str());
  thread.m_stopSemaphore.release();
}

}  // namespace esperanza
