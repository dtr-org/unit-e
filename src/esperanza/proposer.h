// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_PROPOSER_H
#define UNITE_ESPERANZA_PROPOSER_H

#include <esperanza/config.h>
#include <key.h>
#include <primitives/block.h>
#include <sync.h>
#include <waiter.h>

#include <map>
#include <string>
#include <thread>
#include <vector>

class CWallet;

namespace esperanza {

template <class T>
struct ProposerAccess;

class Proposer {
  // accessor for unit testing - not a true friend
  template <class T>
  friend struct ProposerAccess;

 public:
  //! the current proposing status per wallet
  enum class Status : int16_t {
    NOT_PROPOSING = 0,
    IS_PROPOSING = 1,
    NOT_PROPOSING_REINDEXING = -1,
    NOT_PROPOSING_IMPORTING = -2,
    NOT_PROPOSING_SYNCING_BLOCKCHAIN = -3,
    NOT_PROPOSING_NO_PEERS = -4,
    NOT_PROPOSING_NOT_ENOUGH_BALANCE = -5,
    NOT_PROPOSING_DEPTH = -6,
    NOT_PROPOSING_WALLET_LOCKED = -7,
    NOT_PROPOSING_LIMITED = -8,
    NOT_PROPOSING_LAGGING_BEHIND = -9,
  };

  //! bookkeeping data per wallet
  struct State {
    Status m_status = Status::NOT_PROPOSING;

    int64_t m_lastCoinStakeSearchTime = 0;

    //! for regtest, don't stake above nStakeLimitHeight
    int m_stakeLimitHeight = 0;

    CAmount m_stakeCombineThreshold = 1000 * UNIT;

    CAmount m_stakeSplitThreshold = 2000 * UNIT;

    size_t m_maxStakeCombine = 3;
  };

  Proposer(
      //! [in] a name to derive thread names from (groupName-1, groupName-2,
      //! ...)
      const Config &,
      //! [in] a reference to all wallets to propose from
      const std::vector<CWallet *> &wallets);

  ~Proposer();

  //! stops the running proposer threads.
  void Stop();

  //! unleashes the initially reined proposer threads
  void Start();

 private:
  //! a Proposer::Thread captures all the pesky technical details regarding
  //! synchronization, starting, stopping, ...
  struct Thread {
    //! a name for this thread
    const std::string m_threadName;

    //! unmodifiable reference to esperanza configuration
    const Config &m_config;

    //! will be set to true to stop the thread
    std::atomic<bool> m_interrupted;

    //! waited upon to pace proposing, occasionally used to wake up a proposer
    //! thread by RPC calls or changes in chainstate.
    Waiter m_waiter;

    //! the wallets which this thread is responsible for to propose from.
    std::vector<CWallet *> m_wallets;

    //! a semaphore for synchronizing initialization
    CountingSemaphore &m_initSemaphore;

    //! a semaphore for synchronizing start events
    CountingSemaphore &m_startSemaphore;

    //! a semaphore for synchronizing stop events
    CountingSemaphore &m_stopSemaphore;

    //! the actual backing thread
    std::thread m_thread;

    Thread(
        //! [in] a name for this thread.
        const std::string &,
        //! [in] a reference to the global esperanza config
        const Config &,
        //! [in] the wallets which this thread is responsible for.
        const std::vector<CWallet *> &,
        //! a semaphore for synchronizing initialization
        CountingSemaphore &,
        //! a semaphore for synchronizing start events
        CountingSemaphore &,
        //! a semaphore for synchronizing stop events
        CountingSemaphore &);

    //! stops this thread by setting m_interrupted to true and waking it
    void Stop();

    //! wakes this thread from sleeping if it is pacing right now
    void Wake();

    //! sets the status for a specific or for all wallets
    void SetStatus(Status status, CWallet *wallet = nullptr);

    template <typename R, typename P>
    void Sleep(std::chrono::duration<R, P> duration) {
      m_waiter.WaitUpTo(duration);
    }
  };

  //! a semaphore for synchronizing initialization
  CountingSemaphore m_initSemaphore;

  //! a semaphore for synchronizing start events
  CountingSemaphore m_startSemaphore;

  //! a semaphore for synchronizing stop events
  CountingSemaphore m_stopSemaphore;

  const std::vector<std::unique_ptr<Thread>> m_threads;

  static std::vector<std::unique_ptr<Proposer::Thread>> CreateProposerThreads(
      const Config &config, const std::vector<CWallet *> &wallets,
      CountingSemaphore &initSemaphore, CountingSemaphore &startSemaphore,
      CountingSemaphore &stopSemaphore);

  static void Run(Thread &);
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_PROPOSER_H
