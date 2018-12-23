// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_COIN_H
#define UNIT_E_COIN_H

#include <amount.h>
#include <blockchain/blockchain_types.h>
#include <uint256.h>

namespace staking {

//! \brief A brief that is potentially stakeable.
struct Coin {
  //! The id of the transaction this coin is referring to.
  uint256 txid;

  //! The index of the output in the transaction referred to by txid.
  uint32_t index;

  //! The amount of the index-th output in the referenced transaction.
  CAmount amount;

  //! The depth of the piece of stake that is used as Proof-of-Stake in the
  //! newly proposed block. This depth is relative to the currently active
  //! chain's height. The depth of the tip of the chain is one by definition.
  //! Depth zero does not exist.
  //! This has the nice property that the actual height of the stake is
  //!   target_height - depth
  //! To illustrate:
  //!   A --> B --> C --> D   ~~> E
  //! where
  //!   A is the genesis block
  //!   B is the block with the staking output
  //!   D is the current tip
  //!   E is the block to be proposed
  //! The stake in B is at depth 3 with respect to D, a transaction in D
  //! is at depth 1 (by definition the tip is depth=1). The heights are:
  //!   h(A)=0, h(B)=1, h(C)=2, h(D)=3, h(E)=4
  //! The depths with respect to the current tip D are:
  //!   d(A)=4, d(B)=3, d(C)=2, d(D)=1
  //! E does not have a depth yet as it is not part of a chain. So the height
  //! of the current chain is 3 (it's size is 4), the target_height is 4, and
  //! the height of B (having the staking output) is 1, which is 4 minus 3.
  //! The depth of genesis is always the size (not the height) of the chain.
  blockchain::Depth depth;
};

}  // namespace staking

#endif  //UNIT_E_COIN_H
