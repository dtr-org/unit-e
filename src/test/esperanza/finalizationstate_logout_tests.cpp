// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/esperanza/finalizationstate_utils.h>

BOOST_FIXTURE_TEST_SUITE(finalizationstate_logout_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(validate_logout_not_a_validator) {

  FinalizationStateSpy spy;

  BOOST_CHECK_EQUAL(spy.ValidateLogout(RandValidatorAddr()),
                    +Result::LOGOUT_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(validate_logout_before_start_dynasty) {

  FinalizationStateSpy spy;
  uint160 validator_address = RandValidatorAddr();
  CAmount deposit_size = spy.MinDepositSize();

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, deposit_size), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, deposit_size);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validator_address), +Result::LOGOUT_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(validate_logout_already_logged_out) {

  FinalizationStateSpy spy;
  uint160 validator_address = RandValidatorAddr();
  CAmount deposit_size = spy.MinDepositSize();

  // For simplicity we keep the target_hash constant since it does not
  // affect the state.
  uint256 target_hash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &target_hash;
  spy.SetRecommendedTarget(block_index);

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, deposit_size), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, deposit_size);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 1 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 2 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 3 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 4 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetActiveFinalizers().size(), 1);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 5 * spy.EpochLength()), +Result::SUCCESS);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validator_address), +Result::SUCCESS);
  spy.ProcessLogout(validator_address);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 6 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 7 * spy.EpochLength()), +Result::SUCCESS);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validator_address), +Result::LOGOUT_ALREADY_DONE);
}

BOOST_AUTO_TEST_CASE(process_logout_end_dynasty) {

  FinalizationStateSpy spy;
  uint160 validator_address = RandValidatorAddr();
  CAmount deposit_size = spy.MinDepositSize();

  // For simplicity we keep the target_hash constant since it does not
  // affect the state.
  uint256 target_hash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &target_hash;
  spy.SetRecommendedTarget(block_index);

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, deposit_size),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, deposit_size);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 1 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 2 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 3 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 4 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetActiveFinalizers().size(), 1);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 5 * spy.EpochLength()), +Result::SUCCESS);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validator_address), +Result::SUCCESS);
  spy.ProcessLogout(validator_address);

  std::map<uint160, Validator> validators = spy.Validators();
  Validator validator = validators.find(validator_address)->second;
  BOOST_CHECK_EQUAL(704, validator.m_end_dynasty);
}

BOOST_AUTO_TEST_SUITE_END()
