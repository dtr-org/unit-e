// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/snapshot_validation.h>

#include <script/script.h>

namespace snapshot {

bool ValidateCandidateBlockTx(const CTransaction &tx,
                              const CBlockIndex *blockIndex,
                              const CCoinsViewCache &view) {
  // special case for genesis block
  if (blockIndex->nHeight == 0) {
    return true;
  }

  if (!tx.IsCoinBase()) {
    return true;
  }

  assert(!tx.vin.empty());

  CScript script = tx.vin[0].scriptSig;

  opcodetype op;
  std::vector<uint8_t> buf;

  CScript::const_iterator it = script.begin();
  if (!script.GetOp(it, op, buf)) {  // skip height
    return false;
  }

  if (!script.GetOp(it, op, buf)) {  // read snapshot hash
    return false;
  }

  if (buf.size() != 32) {
    return false;
  }

  uint256 sm = blockIndex->pprev->stake_modifier;
  return view.GetSnapshotHash().GetHash(sm) == uint256(buf);
}

bool ReadSnapshotHashFromTx(const CTransaction &tx,
                            uint256 &snapshotHashOut) {
  if (!tx.IsCoinBase()) {
    return false;
  }

  assert(!tx.vin.empty());

  CScript script = tx.vin[0].scriptSig;

  opcodetype op;
  std::vector<uint8_t> buf;

  CScript::const_iterator it = script.begin();
  if (!script.GetOp(it, op, buf)) {  // skip height
    return false;
  }

  if (!script.GetOp(it, op, buf)) {  // read snapshot hash
    return false;
  }

  if (buf.size() != 32) {
    return false;
  }

  snapshotHashOut = uint256(buf);
  return true;
}

}  // namespace snapshot
