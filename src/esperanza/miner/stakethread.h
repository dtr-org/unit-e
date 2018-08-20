// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNITE_ESPERANZA_MINER_STAKETHREAD_H
#define UNITE_ESPERANZA_MINER_STAKETHREAD_H

#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <string>

#include <esperanza/config.h>

class CWallet;

namespace esperanza {

namespace miner {

class StakeThread final {

 private:

  std::string m_name;

  std::thread m_thread;

  std::condition_variable m_condMinerProc;

  std::mutex m_mtxMinerProc;

  bool m_wakeMinerProc = false;

  static void condWaitFor(size_t threadID, int ms);

 public:

  StakeThread(std::string name, std::thread &thread);

  //! Stops all active StakeThreads.
  static void Shutdown();

  //! Wakes the thread associated with the given wallet.
  static void Wake(CWallet *wallet);

  //! Returns true iff there are no active StakeThreads.e
  static bool IsStopped();

  //! Starts a thread with the given id for the given range of wallets.
  static void Start(size_t nThreadID, std::vector<CWallet *> &wallets, size_t start, size_t end);

  //! Given a configuration and a list of wallets, starts staking with one ore more threads.
  static void StartStaking(const esperanza::Config &config, const std::vector<CWallet *> &wallets);

  void condWaitFor(int ms);

};

} // namespace miner

} // namespace esperanza

#endif // UNITE_ESPERANZA_MINER_STAKETHREAD_H
