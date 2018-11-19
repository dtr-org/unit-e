// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_PROPOSER_PROPOSER_H
#define UNITE_PROPOSER_PROPOSER_H

#include <better-enums/enum.h>
#include <dependency.h>
#include <esperanza/settings.h>
#include <key.h>
#include <primitives/block.h>
#include <proposer/blockproposer.h>
#include <proposer/chainstate.h>
#include <proposer/multiwallet.h>
#include <proposer/network.h>
#include <proposer/proposer_settings.h>
#include <proposer/proposer_status.h>
#include <proposer/sync.h>
#include <proposer/waiter.h>

#include <stdint.h>
#include <map>
#include <string>
#include <thread>
#include <vector>

class CWallet;

namespace proposer {

// a stub for testing – specializations of this class have access to the
// Proposer's guts
template <class T>
struct ProposerAccess;

class Proposer {
  // accessor for unit testing - not a true friend
  template <class T>
  friend struct ProposerAccess;

 public:
  static std::unique_ptr<Proposer> MakeProposer(Dependency<Settings>,
                                                Dependency<MultiWallet>,
                                                Dependency<Network>,
                                                Dependency<ChainState>,
                                                Dependency<BlockProposer>);

  Proposer(Dependency<Settings>, Dependency<MultiWallet>, Dependency<Network>,
           Dependency<ChainState>, Dependency<BlockProposer>);

  ~Proposer();

  //! wakes all proposers or the thread which is proposing for the specified
  //! wallet.
  void Wake(const CWallet *wallet = nullptr);

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

    //! reference to parent proposer
    Proposer &m_proposer;

    //! will be set to true to stop the thread
    std::atomic<bool> m_interrupted;

    //! waited upon to pace proposing, occasionally used to wake up a proposer
    //! thread by RPC calls or changes in chainstate.
    Waiter m_waiter;

    //! the wallets which this thread is responsible for to propose from.
    std::vector<CWallet *> m_wallets;

    Thread(
        //! [in] a name for this thread.
        const std::string &,
        //! [in] a reference to the parent proposer
        Proposer &,
        //! [in] the wallets which this thread is responsible for.
        const std::vector<CWallet *> &);

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

  Dependency<Settings> m_settings;
  Dependency<Network> m_network;
  Dependency<ChainState> m_chain;
  Dependency<BlockProposer> m_blockProposer;

  //! a semaphore for synchronizing initialization
  CountingSemaphore m_initSemaphore;

  //! a semaphore for synchronizing start events
  CountingSemaphore m_startSemaphore;

  //! a semaphore for synchronizing stop events
  CountingSemaphore m_stopSemaphore;

  const std::vector<std::unique_ptr<Thread>> m_threads;

  std::vector<std::unique_ptr<Proposer::Thread>> CreateProposerThreads(
      Dependency<MultiWallet>);

  static void Run(Thread &);
};

}  // namespace proposer

#endif  // UNITE_PROPOSER_PROPOSER_H
