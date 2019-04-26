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
  Dependency<proposer::FinalizationRewardLogic> m_finalization_reward_logic;

 public:
  BlockRewardValidatorImpl(
      Dependency<blockchain::Behavior> behavior,
      Dependency<proposer::FinalizationRewardLogic> finalization_reward_logic)
      : m_behavior(behavior),
        m_finalization_reward_logic(finalization_reward_logic) {}

  bool CheckBlockRewards(const CTransaction &coinbase_tx, CValidationState &state, const CBlockIndex &index,
                         CAmount input_amount, CAmount fees) const override {
    assert(MoneyRange(input_amount));
    assert(MoneyRange(fees));

    const CBlockIndex &prev_block = *index.pprev;
    CAmount total_reward = fees + m_behavior->CalculateBlockReward(index.nHeight);

    std::size_t num_reward_outputs = m_finalization_reward_logic->GetNumberOfRewardOutputs(index.nHeight) + 1;
    if (coinbase_tx.vout.size() < num_reward_outputs) {
      return state.DoS(100,
                       error("%s: too few coinbase outputs expected at least %d actual %d", __func__,
                             num_reward_outputs, coinbase_tx.vout.size()),
                       REJECT_INVALID, "bad-cb-too-few-outputs");
    }

    if (num_reward_outputs > 1 && !(prev_block.pprev->nStatus & BLOCK_HAVE_DATA)) {
      // prev_block is a parent block of the snapshot which was used for ISD.
      // We do not have data for the ancestor blocks of prev_block.
      // TODO UNIT-E: implement proper validation of finalization rewards after ISD
      LogPrintf("WARNING: %s partial validation of finalization rewards, block hash=%s\n", __func__,
                HexStr(index.GetBlockHash()));
      std::vector<CAmount> fin_rewards = m_finalization_reward_logic->GetFinalizationRewardAmounts(prev_block);
      for (std::size_t i = 0; i < fin_rewards.size(); ++i) {
        total_reward += fin_rewards[i];
        if (coinbase_tx.vout[i + 1].nValue != fin_rewards[i]) {
          return state.DoS(100, error("%s: incorrect finalization reward", __func__), REJECT_INVALID,
                           "bad-cb-finalization-reward");
        }
      }
    } else if (num_reward_outputs > 1) {
      std::vector<std::pair<CScript, CAmount>> fin_rewards =
          m_finalization_reward_logic->GetFinalizationRewards(prev_block);
      for (std::size_t i = 0; i < fin_rewards.size(); ++i) {
        total_reward += fin_rewards[i].second;
        if (coinbase_tx.vout[i + 1].nValue != fin_rewards[i].second ||
            coinbase_tx.vout[i + 1].scriptPubKey != fin_rewards[i].first) {
          return state.DoS(100, error("%s: incorrect finalization reward", __func__), REJECT_INVALID,
                           "bad-cb-finalization-reward");
        }
      }
    }

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
    Dependency<blockchain::Behavior> behavior,
    Dependency<proposer::FinalizationRewardLogic> finalization_reward_logic) {
  return MakeUnique<BlockRewardValidatorImpl>(behavior, finalization_reward_logic);
}
}  // namespace staking
