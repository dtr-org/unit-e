// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_BLOCK_BUILDER_H
#define UNIT_E_PROPOSER_BLOCK_BUILDER_H

#include <blockchain/blockchain_behavior.h>
#include <dependency.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <proposer/eligible_coin.h>
#include <settings.h>
#include <staking/transactionpicker.h>

#include <memory>

class COutput;
class CWallet;

namespace proposer {

class BlockBuilder {

 public:
  //! \brief Builds a brand new block.
  virtual std::shared_ptr<const CBlock> BuildBlock(
      const CBlockIndex &,                   //!< The previous block / current tip
      const uint256 &,                       //!< The snapshot hash to be included in the new block
      const EligibleCoin &,                  //!< The coin to use as stake
      const std::vector<COutput> &,          //!< Other coins to combine with the stake
      const std::vector<CTransactionRef> &,  //!< Transactions to include in the block
      CAmount,                               //!< The fees on the transactions
      CWallet &                              //!< A wallet used to sign blocks and stake
      ) const = 0;

  virtual ~BlockBuilder() = default;

  static std::unique_ptr<BlockBuilder> New(
      Dependency<blockchain::Behavior>,
      Dependency<Settings>);
};

}  // namespace proposer

#endif  //UNIT_E_BLOCK_BUILDER_H
