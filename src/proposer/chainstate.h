// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_CHAININTERFACE_H
#define UNIT_E_CHAININTERFACE_H

#include <chainparams.h>
#include <primitives/block.h>
#include <sync.h>
#include <sync_status.h>

#include <stdint.h>
#include <memory>
#include <mutex>

namespace proposer {

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
class ChainState {

 public:
  //! \brief access to the mutex that protects chain functions
  virtual CCriticalSection &GetLock() const = 0;

  //! \brief returns the height of the currently active chain.
  //!
  //! If the chain contains only the genesis block then this method
  //! returns 0 (the height of the genesis block). Note that there are
  //! N + 1 blocks in a chain of height N.
  //!
  //! Throws an exception if there is no chain yet. This can only be
  //! the case if the chain interface is invoked before the node has
  //! just started up and didn't have a chance to load blockchain from
  //! disk. While designing this API this was considered to be better
  //! behavior than simply leaving it undefined.
  //!
  //! This function is a drop-in replacement for
  //! <validation.h>::chainActive.Height()
  virtual uint32_t GetHeight() const = 0;

  //! \brief returns the currently active chain's tip.
  //!
  //! Returns a shared pointer to the block which is at the currently
  //! active chains tip. If the chain contains only the genesis block
  //! then a pointer to that is returned.
  //!
  //! Throws an exception if there is no chain yet. This can only be
  //! the case if the chain interface is invoked before the node has
  //! just started up and didn't have a chance to load blockchain from
  //! disk. While designing this API this was considered to be better
  //! behavior than simply leaving it undefined.
  //!
  //! This function is a replacement for
  //! <validation.h>::chainActive.Tip()->GetBlockHeader()
  virtual std::unique_ptr<const CBlockHeader> GetTip() const = 0;

  //! \brief add a new block at the current active chains tip.
  virtual bool ProcessNewBlock(std::shared_ptr<const CBlock> pblock) = 0;

  //! \brief Check the current status of the initial block download.
  virtual ::SyncStatus GetInitialBlockDownloadStatus() const = 0;

  //! \brief Return the blockchain parameters currently active.
  virtual const CChainParams &GetChainParams() const = 0;

  virtual ~ChainState() = default;

  //! \brief Factory method for creating a Chain.
  static std::unique_ptr<ChainState> MakeChain();
};

}  // namespace proposer

#endif  // UNIT_E_CHAININTERFACE_H
