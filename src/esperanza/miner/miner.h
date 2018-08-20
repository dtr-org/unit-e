// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNITE_MINER_H
#define UNITE_MINER_H

#include <atomic>
#include <primitives/block.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>

class CWallet;

namespace esperanza {

namespace miner {

class StakeThread
{
 public:
  void condWaitFor(int ms);

  std::thread thread;
  std::condition_variable condMinerProc;
  std::mutex mtxMinerProc;
  std::string sName;
  bool fWakeMinerProc = false;
};

extern std::vector<StakeThread*> g_takeThreads;

extern std::atomic<bool> g_isStaking;

extern int g_minStakeInterval;

extern int g_minerSleep;

double GetPoSKernelPS();

bool CheckStake(CBlock *pblock);

void ShutdownThreadStakeMiner();

void WakeThreadStakeMiner(CWallet *pwallet);

bool ThreadStakeMinerStopped();

void ThreadStakeMiner(size_t nThreadID, std::vector<CWallet *> &vpwallets, size_t nStart, size_t nEnd);

} // namespace miner

} // namespace esperanza

#endif // UNITE_MINER_H
