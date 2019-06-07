// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#ifndef UNITE_STAKING_BLOCK_REWARD_VALIDATOR_H
#define UNITE_STAKING_BLOCK_REWARD_VALIDATOR_H

#include <amount.h>
#include <blockchain/blockchain_behavior.h>
#include <dependency.h>

#include <memory>

class CBlockIndex;
class CTransaction;
class CValidationState;

namespace staking {

class BlockRewardValidator {
 public:
  virtual bool CheckBlockRewards(
      const CTransaction &coinbase_tx,
      CValidationState &state,
      const CBlockIndex &index,
      CAmount input_amount,
      CAmount fees) const = 0;

  virtual ~BlockRewardValidator() = default;

  static std::unique_ptr<BlockRewardValidator> New(
      Dependency<blockchain::Behavior>);
};

}  // namespace staking

#endif  // UNITE_STAKING_BLOCK_REWARD_VALIDATOR_H
