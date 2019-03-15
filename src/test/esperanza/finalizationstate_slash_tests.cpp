// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/esperanza/finalizationstate_utils.h>

BOOST_FIXTURE_TEST_SUITE(finalizationstate_slash_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(is_slashable_not_a_validator) {

  FinalizationStateSpy spy;
  uint160 validator_address = RandValidatorAddr();
  CAmount deposit_size = spy.MinDepositSize();
  Vote v1{validator_address, uint256S("5"), 3, 5};
  Vote v2{validator_address, uint256S("15"), 3, 5};

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SLASH_NOT_A_VALIDATOR);

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, deposit_size), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, deposit_size);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);

  v1 = {RandValidatorAddr(), uint256S("5"), 3, 5};
  v2 = {validator_address, uint256S("15"), 3, 5};

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SLASH_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(is_slashable_not_the_same_validator) {

  FinalizationStateSpy spy;
  uint160 validator_address_1 = RandValidatorAddr();
  uint160 validator_address_2 = RandValidatorAddr();
  CAmount deposit_size_1 = spy.MinDepositSize();
  CAmount deposit_size_2 = spy.MinDepositSize() + 1;

  Vote v1{validator_address_1, uint256S("5"), 3, 5};
  Vote v2{validator_address_2, uint256S("6"), 12, 52};

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address_1, deposit_size_1), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address_1, deposit_size_1);
  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address_2, deposit_size_2), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address_2, deposit_size_2);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SLASH_NOT_SAME_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(is_slashable_too_early) {

  FinalizationStateSpy spy;
  uint160 validator_address = RandValidatorAddr();
  CAmount deposit_size = spy.MinDepositSize();

  Vote v1{validator_address, uint256S("5"), 3, 5};
  Vote v2{validator_address, uint256S("6"), 12, 52};

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, deposit_size), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, deposit_size);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.IsSlashable(v2, v1), +Result::SLASH_TOO_EARLY);
}

BOOST_AUTO_TEST_CASE(is_slashable_same_vote) {

  FinalizationStateSpy spy;
  uint160 validator_address = RandValidatorAddr();
  CAmount deposit_size = spy.MinDepositSize();
  Vote v1 = {validator_address, uint256S("5"), 3, 5};

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, deposit_size), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, deposit_size);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 1 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 2 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 3 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 4 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetActiveFinalizers().size(), 1);

  // For simplicity we keep the target_hash constant since it does not
  // affect the state.
  uint256 target_hash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &target_hash;
  spy.SetRecommendedTarget(block_index);

  for (uint32_t i = 5; i < 8; ++i) {
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);

    Vote vote{validator_address, target_hash, i - 1, i};

    BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
    spy.ProcessVote(vote);
  }

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v1), +Result::SLASH_SAME_VOTE);
}

BOOST_AUTO_TEST_CASE(is_slashable_already_slashed) {

  FinalizationStateSpy spy;
  uint160 validator_address = RandValidatorAddr();
  CAmount deposit_size = spy.MinDepositSize();

  Vote v1{validator_address, uint256S("5"), 3, 5};
  Vote v2{validator_address, uint256S("6"), 3, 5};

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, deposit_size), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, deposit_size);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 1 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 2 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 3 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 4 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetActiveFinalizers().size(), 1);

  // For simplicity we keep the target_hash constant since it does not
  // affect the state.
  uint256 target_hash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &target_hash;
  spy.SetRecommendedTarget(block_index);

  uint32_t i = 5;
  for (; i < 8; ++i) {
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);

    Vote vote{validator_address, target_hash, i - 1, i};

    BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
    spy.ProcessVote(vote);
  }

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SUCCESS);
  spy.ProcessSlash(v1, v2);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SLASH_ALREADY_SLASHED);
}

BOOST_AUTO_TEST_CASE(process_slash_duplicate_vote) {

  FinalizationStateSpy spy;
  uint160 validator_address = RandValidatorAddr();
  CAmount deposit_size = spy.MinDepositSize();

  // This is a double vote
  Vote v1{validator_address, uint256S("5"), 3, 5};
  Vote v2{validator_address, uint256S("6"), 3, 5};

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, deposit_size), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, deposit_size);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 1 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 2 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 3 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 4 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetActiveFinalizers().size(), 1);

  // For simplicity we keep the target_hash constant since it does not
  // affect the state.
  uint256 target_hash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &target_hash;
  spy.SetRecommendedTarget(block_index);

  for (uint32_t i = 5; i < 8; ++i) {
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()),
                      +Result::SUCCESS);

    Vote vote{validator_address, target_hash, i - 1, i};

    BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
    spy.ProcessVote(vote);
  }

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SUCCESS);
  spy.ProcessSlash(v1, v2);

  CAmount totalDeposit = spy.GetDepositSize(validator_address);
  BOOST_CHECK_EQUAL(0, totalDeposit);
}

BOOST_AUTO_TEST_CASE(process_slash_surrounding_vote) {

  FinalizationStateSpy spy;
  uint160 validator_address = RandValidatorAddr();
  CAmount deposit_size = spy.MinDepositSize();

  // This is a surrounding
  Vote v1{validator_address, uint256S("5"), 1, 5};
  Vote v2{validator_address, uint256S("4"), 3, 4};

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, deposit_size), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, deposit_size);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 1 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 2 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 3 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 4 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetActiveFinalizers().size(), 1);

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 target_hash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &target_hash;
  spy.SetRecommendedTarget(block_index);

  for (uint32_t i = 5; i < 8; ++i) {
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);

    Vote vote{validator_address, target_hash, i - 1, i};

    BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
    spy.ProcessVote(vote);
  }

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SUCCESS);
  spy.ProcessSlash(v1, v2);

  CAmount totalDeposit = spy.GetDepositSize(validator_address);
  BOOST_CHECK_EQUAL(0, totalDeposit);
}

BOOST_AUTO_TEST_SUITE_END()
