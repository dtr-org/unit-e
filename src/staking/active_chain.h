// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STAKING_ACTIVE_CHAIN_H
#define UNIT_E_STAKING_ACTIVE_CHAIN_H

#include <blockchain/blockchain_behavior.h>
#include <blockchain/blockchain_interfaces.h>
#include <chain.h>
#include <primitives/block.h>
#include <staking/coin.h>
#include <sync.h>
#include <sync_status.h>

#include <boost/optional.hpp>

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
  //! genesis block. If there is no active chain (because the genesis block
  //! is not loaded yet) this function will throw an exception. It is guaranteed
  //! not to throw an exception once we're out of IBD.
  virtual blockchain::Height GetHeight() const = 0;

  //! \brief returns the tip of the currently active chain.
  //!
  //! \return A pointer to the current tip or nullptr if there is no current tip yet.
  virtual const CBlockIndex *GetTip() const = 0;

  //! \brief returns the chain genesis.
  //!
  //! If invoked during IBD the genesis block might not have been loaded
  //! from disk yet.
  //!
  //! \return A pointer to the genesis block's index or nullptr if the block is not loaded yet.
  virtual const CBlockIndex *GetGenesis() const = 0;

  //! \brief returns whether chain contains a block index
  virtual bool Contains(const CBlockIndex &) const = 0;

  //! \brief returns most common index between fork and the active chain
  //!
  //! It's what activeChain.FindFork does.
  virtual const CBlockIndex *FindForkOrigin(const CBlockIndex &fork) const = 0;

  //! \brief returns the successor of the index.
  virtual const CBlockIndex *GetNext(const CBlockIndex &block_index) const = 0;

  // defined in blockchain::ChainAccess
  const CBlockIndex *AtDepth(blockchain::Depth) const override = 0;

  // defined in blockchain::ChainAccess
  const CBlockIndex *AtHeight(blockchain::Height) const override = 0;

  //! \brief compute the current respective depth for the given height.
  virtual blockchain::Depth GetDepth(blockchain::Height) const = 0;

  //! \brief lookup a block index entry by its hash/id.
  //!
  //! Requires the lock obtained from `GetLock()` to be held.
  //!
  //! If the block is part of the active chain it is guaranteed to have a
  //! CBlockIndex associated with it.
  //!
  //! If there is no entry in the block index db for this particular block
  //! hash or if the block is not actually part of the active chain
  //! this function will return `nullptr`.
  virtual const CBlockIndex *GetBlockIndex(const uint256 &block_hash) const = 0;

  //! \brief computes the snapshot hash for the current utxo set
  //!
  //! Requires the lock obtained from `GetLock()` to be held.
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
  virtual bool ProcessNewBlock(std::shared_ptr<const CBlock>) = 0;

  //! \brief Check the current status of the initial block download.
  virtual ::SyncStatus GetInitialBlockDownloadStatus() const = 0;

  //! \brief Retrieve a UTXO from the currently active chain.
  //!
  //! The returned coin is guaranteed to represent an _unspent_ tx output
  //! at the point of time where this function is invoked.
  //!
  //! Requires the lock obtained from `GetLock()` to be held.
  virtual boost::optional<staking::Coin> GetUTXO(const COutPoint &) const = 0;

  //! \brief Shorthand for `GetUTXO({ txid, index })`.
  //!
  //! The returned coin is guaranteed to represent an _unspent_ tx output
  //! at the point of time where this function is invoked.
  //!
  //! Requires the lock obtained from `GetLock()` to be held.
  virtual boost::optional<staking::Coin> GetUTXO(const uint256 &txid, const std::uint32_t index) const {
    return GetUTXO({txid, index});
  }

  ~ActiveChain() override = default;

  //! \brief Factory method for creating a Chain.
  static std::unique_ptr<ActiveChain> New();
};

}  // namespace staking

#endif  // UNIT_E_STAKING_ACTIVE_CHAIN_H
