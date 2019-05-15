// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/esperanza/finalizationstate_utils.h>

BOOST_FIXTURE_TEST_SUITE(finalizationstate_logout_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(validate_logout_not_a_validator) {

  finalization::Params params;
  FinalizationStateSpy spy(params);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(RandValidatorAddr()),
                    +Result::LOGOUT_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(validate_logout_before_start_dynasty) {

  finalization::Params params;
  FinalizationStateSpy spy(params);
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize), +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress), +Result::LOGOUT_NOT_YET_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(validate_logout_already_logged_out) {

  finalization::Params params = finalization::Params::TestNet();
  FinalizationStateSpy spy(params);
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(block_index);

  spy.CreateAndActivateDeposit(validatorAddress, depositSize);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress), +Result::SUCCESS);
  spy.ProcessLogout(validatorAddress);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 4 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 5 * spy.EpochLength()), +Result::SUCCESS);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress), +Result::LOGOUT_ALREADY_DONE);
}

BOOST_AUTO_TEST_CASE(process_logout_end_dynasty) {

  finalization::Params params = finalization::Params::TestNet();
  FinalizationStateSpy spy(params);
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(block_index);

  spy.CreateAndActivateDeposit(validatorAddress, depositSize);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress), +Result::SUCCESS);
  spy.ProcessLogout(validatorAddress);

  std::map<uint160, Validator> validators = spy.Validators();
  Validator validator = validators.find(validatorAddress)->second;
  BOOST_CHECK_EQUAL(7, validator.m_end_dynasty);
}

BOOST_AUTO_TEST_SUITE_END()
