// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_FINALIZATION_STATE_PROCESSOR
#define UNITE_FINALIZATION_STATE_PROCESSOR

#include <dependency.h>
#include <primitives/transaction.h>

class CBlock;
class CBlockIndex;

#include <vector>

//! Workflow of the states.
//!
//! Precondition:
//! * Create the 0th and empty state for genesis block (by StateProcessor::ProcessNewTip).
//!
//! The usual life cycle of the state during full sync (longest possible story):
//! 1 [In a process of accepting new commits], call StateStorage::ProcessNewCommits, here:
//!   * Create new state based on its parent state (genesis for 1st). Effectively it's a copy
//!     of parent's state, but it has no new data still, so now it's NEW.
//!   * Fill it with commit transactions for the given block (i.e., call state.ProcessNewCommits)
//!     Now state is filled, but not yet confirmed, it's FROM_COMMITS.
//! 2 [In a process of receiving a real block] call StateStorage::ProcessNewTipCandidate, here:
//!   * Build new state, fill it by transactions given from the block, compare with previous state
//!     that had been built from commits. Now it's COMPLETED.
//! 3 [In the process of switching to the best chain], call StateStorage::ProcessNewTip, here:
//!   * Get previouly processed state (or process new), make sure it's confirmed.
//! 4 Once we reach the next finalized checkpoint, trim all states from old dynasties.
//!
//! Note, the real processing may be started from any of item 1, 2, or 3. For instance,
//! in the usual mode (e.g., compact block) it's started from 2.
//!

namespace finalization {

class StateRepository;

class StateProcessor {
 public:
  //! \brief Create new finalization state for given commits.
  //!
  //! If state exists and not NEW, return true.
  //! Otherwise create new state and process it (parent state must exist and be not NEW, otherwise
  //! returns false). Resulting state's status is FROM_COMMITS.
  //! The final return value is result of calling to esperanza::FinalizationState::ProcessNewTip.
  //!
  virtual bool ProcessNewCommits(const CBlockIndex &block_index, const std::vector<CTransactionRef> &txes) = 0;

  //! \brief Create new finalization state for given block_index.
  //!
  //! If state exists and COMPLETED, return true.
  //! If state exists but processed from COMMITS, reevaluate and compare with one given from commits.
  //! Otherwise, create new one (parent state must exist and be COMPLETED, otherwise return false)
  //! and process it. Resulting state is COMPLETED.
  //! The final return value is result of calling to esperanza::FinalizationState::ProcessNewTip.
  //!
  //! This function is supposed to be called when a block connecting to any chain in the current
  //! dynasty.
  virtual bool ProcessNewTipCandidate(const CBlockIndex &block_index, const CBlock &block) = 0;

  //! \brief Create new finalization state for given block_index.
  //!
  //! If state exists and COMPLETED, return true.
  //! If state exists but processed from COMMITS, reevaluate and compare with one given from commits
  //! Otherwise, create new one (parent state must exist and be COMPLETED, otherwise return false)
  //! and process it. Resulting state is COMPLETED.
  //!
  //! This function is supposed to be called when a block is connecting to the main chain.
  virtual bool ProcessNewTip(const CBlockIndex &block_index, const CBlock &block) = 0;

  virtual ~StateProcessor() = default;
  static std::unique_ptr<StateProcessor> New(Dependency<finalization::StateRepository> repo);
};

}  // namespace finalization

#endif
