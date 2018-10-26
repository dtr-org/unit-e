// Copyright (c) 2018 The unit-e core developers
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

  if (!tx.IsCoinStake()) {
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

  uint256 sm = blockIndex->pprev->bnStakeModifier;
  return view.GetSnapshotHash().GetHash(sm) == uint256(buf);
}

}  // namespace snapshot
