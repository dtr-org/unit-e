// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_FINALIZATION_STATE_REPOSITORY
#define UNITE_FINALIZATION_STATE_REPOSITORY

#include <dependency.h>
#include <esperanza/finalizationstate.h>

#include <memory>

//! Finalization state of every CBlockIndex in the current dynasty is stored. Once being
//! processed, it's stored unless next checkpoint is finalized. Every state is a copy of
//! the previous one plus new finalized commits given from corresponding block. During
//! lifetime state changes its status: NEW -> [ FROM_COMMITS -> ] COMPLETED.
//!
//! Every finalization state is associated with one CBlockIndex (in the current dynasty).
//! Parent state means the state of the CBlockIndex.pprev. States must be processed
//! index by index.

namespace esperanza {
struct FinalizationParams;
struct AdminParams;
}  // namespace esperanza

namespace staking {
class ActiveChain;
class BlockIndexMap;
}  // namespace staking

class CBlockIndex;
class BlockDB;

namespace finalization {

class StateDB;
class StateProcessor;

class MissedBlockError : public std::runtime_error {
 public:
  const CBlockIndex &missed_index;
  MissedBlockError(const CBlockIndex &index)
      : std::runtime_error(strprintf("Cannot load block=%s", index.GetBlockHash().GetHex())),
        missed_index(index) {}
};

// UNIT-E TODO: FinalizationState is gonna be moved to finalization namespace.
// When it happen, remove this line.
using FinalizationState = esperanza::FinalizationState;

class StateRepository {
 public:
  //! Return the finalization state of the current active chain tip.
  virtual FinalizationState *GetTipState() = 0;

  //! Return the finalization state of the given block_index.
  virtual FinalizationState *Find(const CBlockIndex &block_index) = 0;

  //! Returns the finalization state of the given block_index, or create new one.
  //!
  //! To create new state its parent must exist and be as goog as required_parent_status.
  //! When new state is created it's a NEW and must be initialized by ProcessNewTip or
  //! ProcessNewCommits.
  virtual FinalizationState *FindOrCreate(const CBlockIndex &block_index,
                                          FinalizationState::InitStatus required_parent_status) = 0;

  //! \brief Confirm the state.
  //!
  //! The `state` must be a state processed from the block. This function fetches previous state
  //! of the same index processed from commits, and replaces it by new state. Return the result
  //! of comparison between new and previous state.
  virtual bool Confirm(const CBlockIndex &block_index,
                       FinalizationState &&new_state,
                       FinalizationState **state_out) = 0;

  //! \brief Restore the repository for actual active chain.
  //!
  //! Must be called during startup of the node to restore repository for current active chain.
  //!
  //! The way we recover repository requires us to use processor dependency.
  //! We don't use it as a class component to avoid circular dependencies.
  virtual bool RestoreFromDisk(Dependency<finalization::StateProcessor> proc) = 0;

  //! \brief Saves the repository to disk.
  virtual bool SaveToDisk() = 0;

  //! \brief Resturns whether node is reconstructing the repository.
  virtual bool Restoring() const = 0;

  //! \brief Reset the repository.
  //! This function must be called during initialization of the node.
  //! TODO: encapsulate finalization params in dependency.
  virtual void Reset(const esperanza::FinalizationParams &params,
                     const esperanza::AdminParams &admin_params) = 0;

  //! \brief Reset the repo and initialize empty and COMPLETE state for the tip.
  //!
  //! It's a workaround for prune mode. We will get rid of it by restoring finalization
  //! state from disk.
  //! This function will gone after merging commits full sync (wip PR #525)
  virtual void ResetToTip(const CBlockIndex &block_index) = 0;

  //! \brief Destroy states for indexes with heights less than `height`
  virtual void TrimUntilHeight(blockchain::Height height) = 0;

  //! Return the finalization params
  virtual const esperanza::FinalizationParams &GetFinalizationParams() const = 0;

  //! Return the admin params
  virtual const esperanza::AdminParams &GetAdminParams() const = 0;

  virtual ~StateRepository() = default;

  static std::unique_ptr<StateRepository> New(
      Dependency<staking::BlockIndexMap>,
      Dependency<staking::ActiveChain>,
      Dependency<finalization::StateDB>,
      Dependency<BlockDB>);
};

}  // namespace finalization

#endif
