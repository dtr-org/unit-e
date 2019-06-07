// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_BLOCKCHAIN_BLOCKCHAIN_INTERFACES_H
#define UNITE_BLOCKCHAIN_BLOCKCHAIN_INTERFACES_H

#include <blockchain/blockchain_types.h>
#include <chain.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <staking/coin.h>

// The interfaces defined in this file expose very limited functionality each
// and do not come with implementations. They can be used to express things such
// as the difficulty function (a pure function which is shared by all compilation
// targets).

namespace blockchain {

//! \brief Access to the active chain by height and depth.
class ChainAccess {
 public:
  //! \brief Access CBlockIndexes in the active chain at the given depth.
  //!
  //! The given depth must be greater than or equal to 1.
  //!
  //! \return nullptr if no Block at the given depth exists.
  virtual const CBlockIndex *AtDepth(blockchain::Depth depth) const = 0;

  //! \brief Access CBlockIndexes in the active chain at the given height.
  //!
  //! The given height must be greater than or equal to 0.
  //!
  //! \return nullptr if no Block at the given height exists.
  virtual const CBlockIndex *AtHeight(blockchain::Height height) const = 0;

  virtual ~ChainAccess() = default;
};

class UTXOView {
 public:
  //! \brief Retrieve a UTXO from this UTXOView.
  //!
  //! The returned coin is guaranteed to represent an _unspent_ tx output
  //! at the point of time where this function is invoked.
  virtual boost::optional<staking::Coin> GetUTXO(const COutPoint &outpoint) const = 0;
};

}  // namespace blockchain

#endif  // UNITE_BLOCKCHAIN_BLOCKCHAIN_INTERFACES_H
