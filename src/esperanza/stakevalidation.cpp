// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/params.h>
#include <esperanza/finalizationstate.h>
#include <esperanza/stakevalidation.h>
#include <chainparams.h>
#include <script/interpreter.h>
#include <script/standard.h>
#include <util.h>
#include <validation.h>

namespace esperanza {

static const size_t MAX_STAKE_SEEN_SIZE = 1000;

static std::map<COutPoint, uint256> mapStakeSeen;
static std::list<COutPoint> listStakeSeen;

bool HasIsCoinstakeOp(const CScript &scriptIn) {
  CScript::const_iterator pc = scriptIn.begin();

  if (pc == scriptIn.end()) {
    return false;
  }
  opcodetype opcode;
  std::vector<unsigned char> vchPushValue;

  if (!scriptIn.GetOp(pc, opcode, vchPushValue)) {
    return false;
  }
  return opcode == OP_ISCOINSTAKE;
}

bool GetCoinstakeScriptPath(const CScript &scriptIn, CScript &scriptOut)
{
  CScript::const_iterator pc = scriptIn.begin();
  CScript::const_iterator pend = scriptIn.end();
  CScript::const_iterator pcStart = pc;

  opcodetype opcode;
  std::vector<unsigned char> pushValue;

  bool foundOp = false;
  while (pc < pend) {
    if (!scriptIn.GetOp(pc, opcode, pushValue)) {
      break;
    }
    if (!foundOp && opcode == OP_ISCOINSTAKE) {
      pc++; // skip over if

      pcStart = pc;
      foundOp = true;
      continue;
    }
    if (foundOp && opcode == OP_ELSE) {
      pc--;
      scriptOut = CScript(pcStart, pc);
      return true;
    }
  }

  return false;
}

bool AddToMapStakeSeen(const COutPoint &kernel, const uint256 &blockHash) {
  // Overwrites existing values

  std::pair<std::map<COutPoint, uint256>::iterator, bool> ret;
  ret = mapStakeSeen.insert(std::pair<COutPoint, uint256>(kernel, blockHash));
  if (ret.second == false) { // already exists
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

int GetNumBlocksOfPeers() {
  // todo
  return 0;
}
}  // namespace esperanza
