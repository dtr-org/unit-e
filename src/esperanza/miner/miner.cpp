// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <esperanza/miner/miner.h>
#include <esperanza/validation/validation.h>
#include <rpc/blockchain.h>
#include <validation.h>

#include <atomic>

namespace esperanza {

namespace miner {

void StakeThread::condWaitFor(int ms) {}

std::atomic<bool> g_stopMinerProc(false);
std::atomic<bool> g_tryToSync(false);
std::atomic<bool> g_sStaking(false);

double GetPoSKernelPS() {
  LOCK(cs_main);

  CBlockIndex *pindex = chainActive.Tip();
  CBlockIndex *pindexPrevStake = nullptr;

  int nBestHeight = pindex->nHeight;

  int nPoSInterval = 72;  // blocks sampled
  double dStakeKernelsTriedAvg = 0;
  int nStakesHandled = 0, nStakesTime = 0;

  while (pindex && nStakesHandled < nPoSInterval) {
    if (pindexPrevStake) {
      dStakeKernelsTriedAvg += GetDifficulty(pindexPrevStake) * 4294967296.0;
      nStakesTime += pindexPrevStake->nTime - pindex->nTime;
      nStakesHandled++;
    }
    pindexPrevStake = pindex;
    pindex = pindex->pprev;
  }

  double result = 0;

  if (nStakesTime) result = dStakeKernelsTriedAvg / nStakesTime;

  // if (IsProtocolV2(nBestHeight))
  result *= Params().GetStakeTimestampMask(nBestHeight) + 1;

  return result;
}

bool CheckStake(CBlock *pblock) { return false; }

void ShutdownThreadStakeMiner() {}

void WakeThreadStakeMiner(CWallet *pwallet) {}

bool ThreadStakeMinerStopped() { return false; }

void ThreadStakeMiner(size_t nThreadID, std::vector<CWallet *> &vpwallets,
                      size_t nStart, size_t nEnd) {}

}  // namespace miner

}  // namespace esperanza
