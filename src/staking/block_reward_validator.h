// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#ifndef UNIT_E_STAKING_BLOCK_REWARD_VALIDATOR_H
#define UNIT_E_STAKING_BLOCK_REWARD_VALIDATOR_H

#include <amount.h>
#include <blockchain/blockchain_behavior.h>
#include <dependency.h>
#include <proposer/finalization_reward_logic.h>

#include <memory>

class CBlockIndex;
class CTransaction;
class CValidationState;

namespace staking {

class BlockRewardValidator {
 public:
  //! \brief Check whether coinbase transaction has valid outputs.
  //!
  //! It checks that:
  //! 1. The coinbase transaction has correct finalization reward outputs.
  //! 2. The total output is not bigger than the total input plus the total
  //!    reward.
  //! 3. The total output is not smaller than the total input plus the total
  //!    reward without the fees.
  //! 4. The non-reward output is not bigger than the total input.
  //! \return true if the outputs are valid.
  virtual bool CheckBlockRewards(
      const CTransaction &coinbase_tx,  //!< [in] The coinbase transaction.
      CValidationState &state,          //!< [out] The validation state.
      const CBlockIndex &index,         //!< [in] The block which contains the coinbase transaction.
      CAmount input_amount,             //!< [in] The total input to the coinbase transaction.
      CAmount fees                      //!< [in] The total fees of the transactions in the block.
      ) const = 0;

  virtual ~BlockRewardValidator() = default;

  static std::unique_ptr<BlockRewardValidator> New(
      Dependency<blockchain::Behavior>,
      Dependency<proposer::FinalizationRewardLogic>);
};

}  // namespace staking

#endif  //UNIT_E_STAKING_BLOCK_REWARD_VALIDATOR_H
