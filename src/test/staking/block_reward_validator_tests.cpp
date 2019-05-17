// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#include <blockchain/blockchain_parameters.h>
#include <consensus/validation.h>
#include <staking/block_reward_validator.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

namespace {

struct Fixture {

  const CAmount total_reward = 10 * UNIT;
  const CAmount immediate_reward = 1 * UNIT;

  blockchain::Parameters parameters = [this]() {
    blockchain::Parameters p = blockchain::Parameters::TestNet();
    p.reward = total_reward;
    return p;
  }();

  std::unique_ptr<blockchain::Behavior> b =
      blockchain::Behavior::NewFromParameters(parameters);

  CBlockIndex prev_block = []() {
    CBlockIndex b;
    b.nHeight = 100;
    return b;
  }();

  const uint256 block_hash;
  CBlockIndex block = [this]() {
    CBlockIndex b;
    b.pprev = &prev_block;
    b.nHeight = prev_block.nHeight + 1;
    b.phashBlock = &block_hash;
    return b;
  }();

  CTransaction MakeCoinbaseTx(const std::vector<CAmount> &outputs) {
    const CTxIn meta_input;
    const CTxIn staking_input;

    CMutableTransaction tx;
    tx.SetType(TxType::COINBASE);
    tx.vin = {meta_input, staking_input};
    for (const auto out : outputs) {
      tx.vout.emplace_back(out, CScript());
    }
    return tx;
  }

  std::unique_ptr<staking::BlockRewardValidator> GetBlockRewardValidator() {
    return staking::BlockRewardValidator::New(b.get());
  }
};

void CheckTransactionIsRejected(
    const CTransaction &tx,
    const std::string &rejection_reason,
    const staking::BlockRewardValidator &validator,
    const CBlockIndex &block,
    const CAmount input_amount,
    const CAmount fees) {
  CValidationState validation_state;
  const bool result = validator.CheckBlockRewards(tx, validation_state, block, input_amount, fees);
  BOOST_CHECK(!result);
  BOOST_CHECK(!validation_state.IsValid());
  BOOST_CHECK_EQUAL(validation_state.GetRejectCode(), REJECT_INVALID);
  BOOST_CHECK_EQUAL(validation_state.GetRejectReason(), rejection_reason);
}

}  // namespace

BOOST_AUTO_TEST_SUITE(block_reward_validator_tests)

BOOST_AUTO_TEST_CASE(valid_reward) {
  Fixture f;
  const auto validator = f.GetBlockRewardValidator();

  const CAmount input_amount = 10 * UNIT;
  const CAmount fees = UNIT / 2;

  auto test_valid_outputs = [&](const std::vector<CAmount> outputs) {
    CTransaction tx = f.MakeCoinbaseTx(outputs);
    CValidationState validation_state;

    const bool result = validator->CheckBlockRewards(tx, validation_state, f.block, input_amount, fees);
    BOOST_CHECK(result);
    BOOST_CHECK(validation_state.IsValid());
  };
  test_valid_outputs({f.immediate_reward + fees, input_amount});
  test_valid_outputs({f.immediate_reward + fees, input_amount / 2, input_amount / 2});
  test_valid_outputs({f.immediate_reward + fees + input_amount});
  test_valid_outputs({f.immediate_reward + input_amount});
}

BOOST_AUTO_TEST_CASE(total_output_is_too_large) {
  Fixture f;
  const auto validator = f.GetBlockRewardValidator();

  const CAmount input_amount = 11 * UNIT;
  const CAmount fees = UNIT / 2;

  CheckTransactionIsRejected(
      f.MakeCoinbaseTx({f.immediate_reward + fees + 1, input_amount}),
      "bad-cb-amount", *validator, f.block, input_amount, fees);
  CheckTransactionIsRejected(
      f.MakeCoinbaseTx({f.immediate_reward + fees, input_amount + 1}),
      "bad-cb-amount", *validator, f.block, input_amount, fees);
}

BOOST_AUTO_TEST_CASE(no_outputs) {
  Fixture f;
  const auto validator = f.GetBlockRewardValidator();

  const CAmount input_amount = 11 * UNIT;
  const CAmount fees = UNIT / 2;

  CTransaction tx = f.MakeCoinbaseTx({});
  CheckTransactionIsRejected(tx, "bad-cb-too-few-outputs", *validator, f.block, input_amount, fees);
}

BOOST_AUTO_TEST_CASE(total_output_is_too_small) {
  Fixture f;
  const auto validator = f.GetBlockRewardValidator();

  const CAmount input_amount = 11 * UNIT;
  const CAmount fees = UNIT / 2;

  CTransaction tx = f.MakeCoinbaseTx({0, input_amount});
  CheckTransactionIsRejected(tx, "bad-cb-spends-too-little", *validator, f.block, input_amount, fees);
}

BOOST_AUTO_TEST_CASE(non_reward_output_is_too_large) {
  Fixture f;
  const auto validator = f.GetBlockRewardValidator();

  const CAmount input_amount = 15 * UNIT;
  const CAmount fees = UNIT / 2;

  CTransaction tx = f.MakeCoinbaseTx({f.immediate_reward, input_amount + fees});
  CheckTransactionIsRejected(tx, "bad-cb-spends-too-much", *validator, f.block, input_amount, fees);
}

BOOST_AUTO_TEST_SUITE_END()
