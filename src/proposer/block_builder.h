// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_BLOCK_BUILDER_H
#define UNIT_E_PROPOSER_BLOCK_BUILDER_H

#include <blockchain/blockchain_behavior.h>
#include <dependency.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <proposer/eligible_coin.h>
#include <proposer/finalization_reward_logic.h>
#include <settings.h>
#include <staking/coin.h>
#include <staking/stakingwallet.h>
#include <staking/transactionpicker.h>

#include <memory>

namespace proposer {

class BlockBuilder {

 public:
  //! \brief Builds a coinbase transaction.
  virtual const CTransactionRef BuildCoinbaseTransaction(
      const CBlockIndex &prev_block,                    //!< The previous block / current tip.
      const uint256 &snapshot_hash,                     //!< The snapshot hash to be included.
      const EligibleCoin &eligible_coin,                //!< The eligible coin to reference as stake. Also contains the target height.
      const staking::CoinSet &coins,                    //!< Any other coins that should be combined into the coinbase tx.
      CAmount fees,                                     //!< The amount of fees to be included (for the reward).
      const boost::optional<CScript> &coinbase_script,  //!< The scriptpubkey to be used for the coinbase reward and fees.
      staking::StakingWallet &wallet                    //!< The wallet to be used for signing the transaction.
      ) const = 0;

  //! \brief Builds a brand new block.
  virtual std::shared_ptr<const CBlock> BuildBlock(
      const CBlockIndex &prev_block,                    //!< The previous block / current tip.
      const uint256 &snapshot_hash,                     //!< The snapshot hash to be included in the new block.
      const EligibleCoin &stake_coin,                   //!< The coin to use as stake.
      const staking::CoinSet &coins,                    //!< Other coins to combine with the stake.
      const std::vector<CTransactionRef> &txs,          //!< Transactions to include in the block.
      CAmount fees,                                     //!< The fees on the transactions.
      const boost::optional<CScript> &coinbase_script,  //!< The scriptpubkey to be used for the coinbase reward and fees.
      staking::StakingWallet &wallet                    //!< A wallet used to sign blocks and stake.
      ) const = 0;

  virtual ~BlockBuilder() = default;

  static std::unique_ptr<BlockBuilder> New(
      Dependency<Settings>,
      Dependency<FinalizationRewardLogic>);
};

}  // namespace proposer

#endif  //UNIT_E_BLOCK_BUILDER_H
