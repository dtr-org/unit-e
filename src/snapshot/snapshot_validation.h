// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_SNAPSHOT_VALIDATION_H
#define UNITE_SNAPSHOT_SNAPSHOT_VALIDATION_H

#include <chain.h>
#include <primitives/transaction.h>
#include <txdb.h>

namespace snapshot {

//! ValidateCandidateBlockTx validates the single TX of the candidate block
//! and must be used inside ConnectBlock function
//!
//! \param tx which is being validated
//! \param blockIndex that represents the block which has tx
//! \param view that contains the snapshot hash of the previous block
bool ValidateCandidateBlockTx(const CTransaction &tx,
                              const CBlockIndex *blockIndex,
                              const CCoinsViewCache &view);

bool ReadSnapshotHashFromTx(const CTransaction &tx,
                            uint256 &snapshotHashOut);

}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_SNAPSHOT_VALIDATION_H
