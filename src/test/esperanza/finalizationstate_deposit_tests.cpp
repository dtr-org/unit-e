#include <test/esperanza/finalizationstate_utils.h>

BOOST_FIXTURE_TEST_SUITE(finalizationstate_deposit_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(validate_deposit_tx_not_enough_deposit) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MinDepositSize() - 1;

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorIndex, depositSize),
                    +Result::DEPOSIT_INSUFFICIENT);
}

BOOST_AUTO_TEST_CASE(validate_deposit_tx_double_deposit) {

  FinalizationStateSpy spy;

  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MinDepositSize();

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorIndex, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorIndex, depositSize),
                    +Result::DEPOSIT_ALREADY_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(process_deposit_tx) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MinDepositSize();

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorIndex, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  std::map<uint256, Validator> validators = spy.Validators();
  auto it = validators.find(validatorIndex);
  BOOST_CHECK(it != validators.end());

  Validator validator = it->second;
  BOOST_CHECK_EQUAL(validator.m_startDynasty, 2);  // assuming we start from 0
  BOOST_CHECK(validator.m_deposit > 0);
  BOOST_CHECK_EQUAL(it->first.GetHex(), validatorIndex.GetHex());
}

BOOST_AUTO_TEST_SUITE_END()
