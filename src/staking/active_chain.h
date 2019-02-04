// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STAKING_ACTIVE_CHAIN_H
#define UNIT_E_STAKING_ACTIVE_CHAIN_H

#include <blockchain/blockchain_behavior.h>
#include <blockchain/blockchain_interfaces.h>
#include <chain.h>
#include <primitives/block.h>
#include <sync.h>
#include <sync_status.h>

#include <cstdint>
#include <memory>
#include <mutex>

namespace staking {

//! \brief an interface to the current blockchain's state.
//!
//! Proposing a block requires access to the current chain state. A lot
//! of these mechanisms are free functions in bitcoin which are super
//! hard to control for example in unit tests. Thus this interface is
//! defined to encapsulate all that free floating stuff behind a single
//! API. Inspiration for this has been drawn from a proposed Chain
//! interface in bitcoin to separate Wallet and Node from each other.
//! See: https://github.com/bitcoin/bitcoin/pull/14437 - in particular
//! https://github.com/ryanofsky/bitcoin/blob/45b23efaada081a7be9e255df59670f4704c45d1/src/interfaces/chain.h
class ActiveChain : public blockchain::ChainAccess {

 public:
  //! \brief access to the mutex that protects the active chain
  //!
  //! Usage: LOCK(chain->GetLock())
  //!
  //! This way the existing DEBUG_LOCKORDER and other debugging features can
  //! work as expected.
  virtual CCriticalSection &GetLock() const = 0;

  //! \brief returns the size of the currently active chain.
  //!
  //! If the chain contains only the genesis block then this method
  //! returns 1. Note that there are N blocks in a chain of size N and the
  //! tip has height N - 1 (as the genesis block has height 0 by definition).
  virtual blockchain::Height GetSize() const = 0;

  //! \brief returns the height of the tip of the currently active chain.
  //!
  //! The height of the genesis block is zero. An active chain always has a
  //! genesis block.
  virtual blockchain::Height GetHeight() const = 0;

  virtual const CBlockIndex *GetTip() const = 0;

  // defined in blockchain::ChainAccess
  const CBlockIndex *AtDepth(blockchain::Depth depth) override = 0;

  // defined in blockchain::ChainAccess
  const CBlockIndex *AtHeight(blockchain::Height height) override = 0;

  //! \brief computes the snapshot hash for the current utxo set
  //!
  //! Note that a block contains the snapshot hash of the utxo set at the
  //! time of proposing the new block, i.e. not the snapshot hash of the utxo
  //! set after the transactions in that new block would have been processed.
  //!
  //! This function is thus useful for proposing and validating and can only
  //! be used as long as the block to validate has not been processed into the
  //! coins db yet (the snapshot hash in meta input of the the active chain's
  //! tip block is not the same as the result of this function).
  virtual const uint256 ComputeSnapshotHash() const = 0;

  //! \brief add a new block at the current active chains tip.
  virtual bool ProcessNewBlock(std::shared_ptr<const CBlock> pblock) = 0;

  //! \brief Check the current status of the initial block download.
  virtual ::SyncStatus GetInitialBlockDownloadStatus() const = 0;

  ~ActiveChain() override = default;

  //! \brief Factory method for creating a Chain.
  static std::unique_ptr<ActiveChain> New();
};

}  // namespace staking

#endif  // UNIT_E_STAKING_ACTIVE_CHAIN_H
