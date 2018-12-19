// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_FINALIZATION_CACHE
#define UNITE_FINALIZATION_CACHE

#include <memory>
#include <vector>

#include <primitives/transaction.h>

//! Finalization state of every CBlockIndex in the current dynasty is cached. Once being
//! processed, it's stored unless next checkpoint is finalized. Every state is a copy of
//! the previous one plus new finalized commits given from corresponding block. During
//! lifetime state changes its status: NEW -> [ FROM_COMMITS -> ] CONFIRMED.
//!
//! Every finalization state is associated with one CBlockIndex (in the current dynasty).
//! Parent state means the state of the CBlockIndex.pprev. States must be processes
//! index by index.
//!
//! Workflow of the states.
//!
//! Precondition:
//! * Create the 0th and empty state for genesis block (by cache::ProcessNewTip).
//!
//! The usual life cycle of the state during full sync (longest possible story):
//! 1 [In a process of accepting new commits], call cache::ProcessNewCommits, here:
//!   * Create new state based on its parent state (genesis for 1st). Effectively it's a copy
//!     of parent's state, but it has no new data still, so now it's NEW.
//!   * Fill it with commit transactions for the given block (i.e., call state.ProcessNewCommits)
//!     Now state is filled, but not yet confirmed, it's FROM_COMMITS.
//! 2 [In a process of receiving a real block] call cache::ProcessNewTipCandidate, here:
//!   * Build new state, fill it by transactions given from the block, compare with previous state
//!     that had been built from commits. Now it's CONFIRMED.
//! 3 [In the process of switching to the best chain], call cache::ProcessNewTip, here:
//!   * Get previouly processed state (or process new), make sure it's confirmed.
//! 4 Once we reach the next finalized checkpoint, trim all states from old dynasties.
//!
//! Note, the real processing may be started from any of item 1, 2, or 3. For instance,
//! in the usual mode (e.g., compact block) it's started from 2.
//!
//! !The cache stores only states, it doesn't care about forks!

namespace esperanza {
class FinalizationState;
struct FinalizationParams;
struct AdminParams;
};  // namespace esperanza

class CBlock;
class CBlockIndex;
class CChainParams;

namespace finalization {
namespace cache {

//! Return the finalization state of the current active chain tip.
esperanza::FinalizationState *GetState();

//! Return the finalization state of the given block_index.
esperanza::FinalizationState *GetState(const CBlockIndex &block_index);

//! \brief Create new finalization state for given commits.
//!
//! If state exists and not NEW, return true.
//! Otherwise create new state and process it (parent state must exist and be not NEW, otherwise
//! returns false). Resulting state's status is FROM_COMMITS.
//! The final return value is result of calling to esperanza::FinalizationState::ProcessNewTip.
//!
bool ProcessNewCommits(const CBlockIndex &block_index, const std::vector<CTransactionRef> &txes);

//! \brief Create new finalization state for given block_index.
//!
//! If state exists and CONFIRMED, return true.
//! If state exists but processed from COMMITS, reevaluate and compare with one given from commits.
//! Otherwise, create new one (parent state must exist and be CONFIRMED, otherwise return false)
//! and process it. Resulting state is CONFIRMED.
//! The final return value is result of calling to esperanza::FinalizationState::ProcessNewTip.
//!
//! This function is supposed to be called when a block connecting to any chain in the current
//! dynasty.
bool ProcessNewTipCandidate(const CBlockIndex &block_index, const CBlock &block);

//! \brief Create new finalization state for given block_index.
//!
//! If state exists and CONFIRMED, return true.
//! If state exists but processed from COMMITS, reevaluate and compare with one given from commits
//! Otherwise, create new one (parent state must exist and be CONFIRMED, otherwise return false)
//! and process it. Resulting state is CONFIRMED.
//!
//! This function is supposed to be called when a block is connecting to the main chain.
bool ProcessNewTip(const CBlockIndex &block_index, const CBlock &block);

//! \brief Restore the cache for actual active chain.
//!
//! Must be called during startup of the node to restore cache for current active chain.
void Restore(const CChainParams &chainparams);

//! \brief Reset the cache.
void Reset(const esperanza::FinalizationParams &params,
           const esperanza::AdminParams &admin_params);

}  // namespace cache
}  // namespace finalization

#endif
