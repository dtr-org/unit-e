#include <test/esperanza/finalizationstate_utils.h>

BOOST_FIXTURE_TEST_SUITE(finalizationstate_logout_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(validate_logout_not_a_validator) {

  FinalizationStateSpy spy;

  BOOST_CHECK_EQUAL(spy.ValidateLogout(RandValidatorAddr()),
                    +Result::LOGOUT_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(validate_logout_before_start_dynasty) {

  FinalizationStateSpy spy;
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);
  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress),
                    +Result::LOGOUT_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(validate_logout_already_logged_out) {

  FinalizationStateSpy spy;
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress), +Result::SUCCESS);
  spy.ProcessLogout(validatorAddress);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(4 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(5 * spy.EpochLength()),
                    +Result::SUCCESS);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress),
                    +Result::LOGOUT_ALREADY_DONE);
}

BOOST_AUTO_TEST_CASE(process_logout_end_dynasty) {

  FinalizationStateSpy spy;
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress), +Result::SUCCESS);
  spy.ProcessLogout(validatorAddress);

  std::map<uint160, Validator> validators = spy.Validators();
  Validator validator = validators.find(validatorAddress)->second;
  BOOST_CHECK_EQUAL(702, validator.m_endDynasty);
}

BOOST_AUTO_TEST_SUITE_END()
