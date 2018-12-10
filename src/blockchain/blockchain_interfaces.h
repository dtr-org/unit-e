// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_INTERFACES_H
#define UNIT_E_BLOCKCHAIN_INTERFACES_H

#include <blockchain/blockchain_types.h>
#include <chain.h>
#include <primitives/block.h>

#include <cstdint>

// The interfaces defined in this file expose very limited functionality each
// and do not come with implementations. They can be used to express things such
// as the difficulty function (a pure function which is shared by all compilation
// targets).

namespace blockchain {

//! \brief Traverses the currently active best chain by depth.
//!
//! A DepthIterator always starts at a given block and traverses into the
//! direction of the genesis block. That is: incrementing it (++) moves into
//! the direction of genesis, decrementing it (--) moves into the direction of
//! the tip.
//!
//! A DepthIterator can be checked for whether it is still valid by converting
//! it to a (bool) – an explicit conversion, usable in if-statements, is defined.
class ChainAccess {
 public:
  //! \brief Access CBlockIndexes in the active chain
  //!
  //! Negative values are treated as blocks at the given depth, i.e. -1 would
  //! look for the block at depth 1 which is exactly the chain's tip. Positive
  //! values and zero refer to blocks by height. The block at height 0 is the
  //! genesis block, the block at height 1 the one after that, etc.
  //!
  //! \return nullptr if no Block at the given height/depth exists.
  virtual const CBlockIndex *operator[](std::int64_t) = 0;

  virtual ~ChainAccess() = default;
};

}  // namespace blockchain

#endif  //UNIT_E_BLOCKCHAIN_INTERFACES_H
