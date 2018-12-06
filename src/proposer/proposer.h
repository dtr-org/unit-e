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
#include <proposer/multiwallet.h>
#include <proposer/proposer_settings.h>
#include <proposer/proposer_status.h>
#include <proposer/sync.h>
#include <proposer/waiter.h>
#include <staking/chainstate.h>
#include <staking/network.h>

#include <stdint.h>
#include <map>
#include <string>
#include <thread>
#include <vector>

class CWallet;

namespace proposer {

// a stub for testing – specializations of this class have access to the
// ProposerImpl's guts
template <class T>
struct ProposerAccess;

class Proposer {

 public:
  virtual void Wake() = 0;

  virtual void Wake(const CWallet *) = 0;

  virtual void Start() = 0;

  virtual ~Proposer() = default;

  static std::unique_ptr<Proposer> New(Dependency<Settings>,
                                       Dependency<MultiWallet>,
                                       Dependency<staking::Network>,
                                       Dependency<staking::ChainState>,
                                       Dependency<BlockProposer>);
};

class ProposerStub : public Proposer {
 public:
  void Wake() override {}
  void Wake(const CWallet *) override {}
  void Start() override {}
};

class ProposerImpl : public Proposer {
  // accessor for unit testing - not a true friend
  template <class T>
  friend struct ProposerAccess;

 public:
  ProposerImpl(Dependency<Settings>,
               Dependency<MultiWallet>,
               Dependency<staking::Network>,
               Dependency<staking::ChainState>,
               Dependency<BlockProposer>);

  ~ProposerImpl() override;

  //! \brief shorthand for waking all proposers.
  void Wake() override {
    Wake(nullptr);
  }

  //! \brief wake a specific proposer
  //!
  //! If the passed wallet ptr is null then all proposers are woken up,
  //! otherwise the one which is responsible for managing this wallet.
  void Wake(const CWallet *wallet) override;

  //! unleashes the initially reined proposer threads
  void Start() override;

 private:
  //! a Proposer::Thread captures all the pesky technical details regarding
  //! synchronization, starting, stopping, ...
  struct Thread {
    //! a name for this thread
    const std::string m_thread_name;

    //! reference to parent proposer
    ProposerImpl &m_proposer;

    //! will be set to true to stop the thread
    std::atomic<bool> m_interrupted;

    //! waited upon to pace proposing, occasionally used to wake up a proposer
    //! thread by RPC calls or changes in chainstate.
    Waiter m_waiter;

    //! the wallets which this thread is responsible for to propose from.
    std::vector<CWallet *> m_wallets;

    //! the underlying thread's handle
    std::thread m_thread;

    Thread(
        //! [in] a name for this thread.
        const std::string &,
        //! [in] a reference to the parent proposer
        ProposerImpl &,
        //! [in] the wallets which this thread is responsible for.
        std::vector<CWallet *> &&);

    //! stops this thread by setting m_interrupted to true and waking it
    void Stop();

    //! waits for this thread to actually have stopped
    void Join();

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
  Dependency<MultiWallet> m_multi_wallet;
  Dependency<staking::Network> m_network;
  Dependency<staking::ChainState> m_chain;
  Dependency<BlockProposer> m_block_proposer;

  //! a flag for whether the proposer is started
  std::atomic_flag m_started = ATOMIC_FLAG_INIT;

  //! a semaphore for synchronizing initialization
  CountingSemaphore m_init_semaphore;

  //! a semaphore for synchronizing start events
  CountingSemaphore m_stop_semaphore;

  std::vector<std::unique_ptr<Thread>> m_threads;

  void CreateProposerThreads();

  static void Run(Thread &);
};

}  // namespace proposer

#endif  // UNITE_PROPOSER_PROPOSER_H
