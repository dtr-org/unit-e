// Copyright (c) 2018 The Unit-e developers
// Copyright (c) 2017 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <esperanza/finalizationstate.h>
#include <esperanza/params.h>
#include <script/interpreter.h>
#include <script/standard.h>
#include <staking/kernel.h>
#include <staking/stakevalidation.h>
#include <util.h>
#include <utilmoneystr.h>
#include <validation.h>

namespace staking {

static const size_t MAX_STAKE_SEEN_SIZE = 1000;

static std::map<COutPoint, uint256> mapStakeSeen;
static std::list<COutPoint> listStakeSeen;

bool HasIsCoinstakeOp(const CScript &scriptIn) {
  // UNIT-E: TODO: remove this function
  return false;
}

bool GetCoinstakeScriptPath(const CScript &scriptIn, CScript &scriptOut) {
  // UNIT-E: TODO: remove this function
  return false;
}

bool AddToMapStakeSeen(const COutPoint &kernel, const uint256 &blockHash) {
  // Overwrites existing values

  std::pair<std::map<COutPoint, uint256>::iterator, bool> ret;
  ret = mapStakeSeen.insert(std::pair<COutPoint, uint256>(kernel, blockHash));
  if (!ret.second) {  // already exists
    ret.first->second = blockHash;
  } else {
    listStakeSeen.push_back(kernel);
  }
  return true;
}

bool CheckStakeUnused(const COutPoint &kernel) {
  std::map<COutPoint, uint256>::const_iterator mi = mapStakeSeen.find(kernel);
  if (mi != mapStakeSeen.end()) {
    return false;
  }
  return true;
}

bool CheckStakeUnique(const CBlock &block, bool update) {
  uint256 blockHash = block.GetHash();
  const COutPoint &kernel = block.vtx[0]->vin[0].prevout;

  std::map<COutPoint, uint256>::const_iterator mi = mapStakeSeen.find(kernel);
  if (mi != mapStakeSeen.end()) {
    if (mi->second == blockHash) {
      return true;
    }
    return error("%s: Stake kernel for %s first seen on %s.", __func__,
                 blockHash.ToString(), mi->second.ToString());
  }
  if (!update) {
    return true;
  }
  if (listStakeSeen.size() > MAX_STAKE_SEEN_SIZE) {
    const COutPoint &oldest = listStakeSeen.front();
    if (1 != mapStakeSeen.erase(oldest)) {
      LogPrintf("%s: Warning: mapStakeSeen did not erase %s %n\n", __func__,
                oldest.hash.ToString(), oldest.n);
    }
    listStakeSeen.pop_front();
  }
  return AddToMapStakeSeen(kernel, blockHash);
}

bool ExtractStakingKeyID(const CScript &scriptPubKey, CKeyID &keyID) {
  if (scriptPubKey.IsPayToPublicKeyHash()) {
    keyID = CKeyID(uint160(&scriptPubKey[3], 20));
    return true;
  }
  return false;
}

bool CheckBlock(const CBlock &pblock) {
  uint256 proofHash, hashTarget;
  uint256 hashBlock = pblock.GetHash();

  if (!staking::CheckStakeUnique(pblock, false)) {  // Check in SignBlock also
    return error("%s: %s CheckStakeUnique failed.", __func__,
                 hashBlock.GetHex());
  }

  BlockMap::const_iterator mi = mapBlockIndex.find(pblock.hashPrevBlock);
  if (mi == mapBlockIndex.end()) {
    return error("%s: %s prev block not found: %s.", __func__,
                 hashBlock.GetHex(), pblock.hashPrevBlock.GetHex());
  }
  if (!chainActive.Contains(mi->second)) {
    return error("%s: %s prev block in active chain: %s.", __func__,
                 hashBlock.GetHex(), pblock.hashPrevBlock.GetHex());
  }
  // verify hash target and signature of coinstake tx
  if (!staking::CheckProofOfStake(mi->second, *pblock.vtx[0], pblock.nTime,
                                  pblock.nBits, proofHash, hashTarget)) {
    return error("%s: proof-of-stake checking failed.", __func__);
  }

  // debug print
  LogPrintf(
      "CheckStake(): New proof-of-stake block found  \n  hash: %s "
      "\nproofhash: "
      "%s  \ntarget: %s\n",
      hashBlock.GetHex(), proofHash.GetHex(), hashTarget.GetHex());
  if (LogAcceptCategory(BCLog::VALIDATION)) {
    LogPrintf("block %s\n", pblock.ToString());
    LogPrintf("out %s\n", FormatMoney(pblock.vtx[0]->GetValueOut()));
  }

  {
    LOCK(cs_main);
    if (pblock.hashPrevBlock !=
        chainActive.Tip()->GetBlockHash())  // hashbestchain
      return error("%s: Generated block is stale.", __func__);
  }

  return true;
}

}  // namespace staking
