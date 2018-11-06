// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_SNAPSHOT_VALIDATION_H
#define UNIT_E_SNAPSHOT_VALIDATION_H

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

}  // namespace snapshot

#endif  // UNIT_E_SNAPSHOT_VALIDATION_H
