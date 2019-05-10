// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#include <staking/block_reward_validator.h>

#include <chain.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>

#include <utilmoneystr.h>

namespace staking {

class BlockRewardValidatorImpl : public BlockRewardValidator {
 private:
  Dependency<blockchain::Behavior> m_behavior;

 public:
  BlockRewardValidatorImpl(
      Dependency<blockchain::Behavior> behavior)
      : m_behavior(behavior) {}

  bool CheckBlockRewards(const CTransaction &coinbase_tx, CValidationState &state, const CBlockIndex &index,
                         CAmount input_amount, CAmount fees) override {
    assert(MoneyRange(input_amount));
    assert(MoneyRange(fees));

    CAmount total_reward = fees + m_behavior->CalculateBlockReward(index.nHeight);

    std::size_t num_reward_outputs = 1;

    CAmount output_amount = coinbase_tx.GetValueOut();
    if (output_amount - input_amount > total_reward) {
      return state.DoS(100,
                       error("%s: coinbase pays too much (total output=%d total input=%d expected reward=%d )",
                             __func__, FormatMoney(output_amount), FormatMoney(input_amount),
                             FormatMoney(total_reward)),
                       REJECT_INVALID, "bad-cb-amount");
    }

    // TODO UNIT-E: make the check stricter: if (output_amount - input_amount < total_reward)
    if (output_amount - input_amount < total_reward - fees) {
      return state.DoS(100,
                       error("%s: coinbase pays too little (total output=%d total input=%d expected reward=%d )",
                             __func__, FormatMoney(output_amount), FormatMoney(input_amount),
                             FormatMoney(total_reward)),
                       REJECT_INVALID, "bad-cb-spends-too-little");
    }

    CAmount non_reward_out = 0;
    for (std::size_t i = num_reward_outputs; i < coinbase_tx.vout.size(); ++i) {
      non_reward_out += coinbase_tx.vout[i].nValue;
    }
    if (non_reward_out > input_amount) {
      return state.DoS(100,
                       error("%s: coinbase spends too much (non-reward output=%d total input=%d)", __func__,
                             FormatMoney(non_reward_out), FormatMoney(input_amount)),
                       REJECT_INVALID, "bad-cb-spends-too-much");
    }

    return true;
  }
};

std::unique_ptr<BlockRewardValidator> BlockRewardValidator::New(
    Dependency<blockchain::Behavior> behavior) {
  return MakeUnique<BlockRewardValidatorImpl>(behavior);
}
}  // namespace staking
