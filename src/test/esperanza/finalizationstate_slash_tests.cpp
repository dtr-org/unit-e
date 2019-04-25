// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/esperanza/finalizationstate_utils.h>

BOOST_FIXTURE_TEST_SUITE(finalizationstate_slash_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(is_slashable_not_a_validator) {

  finalization::Params params;
  FinalizationStateSpy spy(params);
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();
  Vote v1 = {validatorAddress, uint256S("5"), 3, 5};
  Vote v2 = {validatorAddress, uint256S("15"), 3, 5};

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SLASH_NOT_A_VALIDATOR);

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize), +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);

  v1 = {RandValidatorAddr(), uint256S("5"), 3, 5};
  v2 = {validatorAddress, uint256S("15"), 3, 5};

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SLASH_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(is_slashable_not_the_same_validator) {

  finalization::Params params;
  FinalizationStateSpy spy(params);
  uint160 validatorAddress_1 = RandValidatorAddr();
  uint160 validatorAddress_2 = RandValidatorAddr();
  CAmount depositSize_1 = spy.MinDepositSize();
  CAmount depositSize_2 = spy.MinDepositSize() + 1;

  Vote v1 = {validatorAddress_1, uint256S("5"), 3, 5};
  Vote v2 = {validatorAddress_2, uint256S("6"), 12, 52};

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress_1, depositSize_1), +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress_1, depositSize_1);
  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress_2, depositSize_2), +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress_2, depositSize_2);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SLASH_NOT_SAME_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(is_slashable_too_early) {

  finalization::Params params;
  FinalizationStateSpy spy(params);
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  Vote v1 = {validatorAddress, uint256S("5"), 3, 5};
  Vote v2 = {validatorAddress, uint256S("6"), 12, 52};

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize), +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.IsSlashable(v2, v1), +Result::SLASH_TOO_EARLY);
}

BOOST_AUTO_TEST_CASE(is_slashable_same_vote) {

  finalization::Params params;
  FinalizationStateSpy spy(params);
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();
  Vote v1 = {validatorAddress, uint256S("5"), 3, 5};

  spy.CreateAndActivateDeposit(validatorAddress, depositSize);

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(block_index);

  for (uint32_t i = 4; i < 6; ++i) {
    Vote vote{validatorAddress, targetHash, i - 2, i - 1};
    BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
    spy.ProcessVote(vote);

    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);
  }

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v1), +Result::SLASH_SAME_VOTE);
}

BOOST_AUTO_TEST_CASE(is_slashable_already_slashed) {

  finalization::Params params;
  FinalizationStateSpy spy(params);
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  Vote v1 = {validatorAddress, uint256S("5"), 3, 5};
  Vote v2 = {validatorAddress, uint256S("6"), 3, 5};

  spy.CreateAndActivateDeposit(validatorAddress, depositSize);

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(block_index);

  uint32_t i = 4;
  for (; i < 6; ++i) {
    Vote vote{validatorAddress, targetHash, i - 2, i - 1};

    BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
    spy.ProcessVote(vote);

    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);
  }

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SUCCESS);
  spy.ProcessSlash(v1, v2);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SLASH_ALREADY_SLASHED);
}

BOOST_AUTO_TEST_CASE(process_slash_duplicate_vote) {

  finalization::Params params;
  FinalizationStateSpy spy(params);
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // This is a double vote
  Vote v1 = {validatorAddress, uint256S("5"), 3, 5};
  Vote v2 = {validatorAddress, uint256S("6"), 3, 5};

  spy.CreateAndActivateDeposit(validatorAddress, depositSize);

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(block_index);

  for (uint32_t i = 4; i < 6; ++i) {
    Vote vote{validatorAddress, targetHash, i - 2, i - 1};
    BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
    spy.ProcessVote(vote);

    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()),
                      +Result::SUCCESS);
  }

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SUCCESS);
  spy.ProcessSlash(v1, v2);

  CAmount totalDeposit = spy.GetDepositSize(validatorAddress);
  BOOST_CHECK_EQUAL(0, totalDeposit);
}

BOOST_AUTO_TEST_CASE(process_slash_surrounding_vote) {

  finalization::Params params;
  FinalizationStateSpy spy(params);
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // This is a surrounding
  Vote v1 = {validatorAddress, uint256S("5"), 1, 5};
  Vote v2 = {validatorAddress, uint256S("4"), 3, 4};

  spy.CreateAndActivateDeposit(validatorAddress, depositSize);

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(block_index);

  for (uint32_t i = 4; i < 6; ++i) {
    Vote vote{validatorAddress, targetHash, i - 2, i - 1};
    BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
    spy.ProcessVote(vote);

    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);
  }

  BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SUCCESS);
  spy.ProcessSlash(v1, v2);

  CAmount totalDeposit = spy.GetDepositSize(validatorAddress);
  BOOST_CHECK_EQUAL(0, totalDeposit);
}

BOOST_AUTO_TEST_SUITE_END()
