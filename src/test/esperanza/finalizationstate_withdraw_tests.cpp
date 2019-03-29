// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/esperanza/finalizationstate_utils.h>

/**
 * Those are the tests for the esperanza finalization state machine.
 * A special notation is introduced when documenting the test behaviour for
 * brevity:
 *  bX -> indicates the Xth block
 *  eX -> indicates the Xth epoch
 *  dX -> indicates the Xth dynasty
 *
 * So i.e. considering an initial setting of EPOCH_LENGTH = 50
 * the notation b210/e4/d2 means that we are currently processing the 210th
 * block that belongs to the 4th epoch and the 2nd dynasty.
 */

BOOST_FIXTURE_TEST_SUITE(finalizationstate_withdraw_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(validate_withdraw_not_a_validator) {

  FinalizationStateSpy spy;
  CAmount withdrawAmount = 0;

  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(RandValidatorAddr(), withdrawAmount),
                    +Result::WITHDRAW_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(process_withdraw_before_end_dynasty) {

  FinalizationStateSpy spy;
  CAmount withdrawAmount = 0;
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

  for (uint32_t i = 6; i < spy.DynastyLogoutDelay() + 1; ++i) {
    Vote vote{validatorAddress, targetHash, i - 2, i - 1};
    BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
    spy.ProcessVote(vote);

    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);
  }

  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validatorAddress, withdrawAmount),
                    +Result::WITHDRAW_BEFORE_END_DYNASTY);
}

BOOST_AUTO_TEST_CASE(process_withdraw_too_early) {

  FinalizationStateSpy spy;
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(block_index);

  spy.CreateAndActivateDeposit(validatorAddress, depositSize);

  // logout
  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress), +Result::SUCCESS);
  spy.ProcessLogout(validatorAddress);

  Validator *validator = &(*spy.pValidators())[validatorAddress];

  uint32_t end_logout = spy.GetCurrentEpoch() + static_cast<uint32_t>(spy.DynastyLogoutDelay());
  uint32_t end_withdraw = end_logout + static_cast<uint32_t>(spy.WithdrawalEpochDelay());

  for (uint32_t i = spy.GetCurrentEpoch(); i < end_withdraw; ++i) {
    if (spy.GetCurrentDynasty() < validator->m_end_dynasty) {
      Vote vote{validatorAddress, targetHash, i - 2, i - 1};

      BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
      spy.ProcessVote(vote);
    }

    if (i <= end_logout) {
      BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validatorAddress, depositSize),
                        +Result::WITHDRAW_BEFORE_END_DYNASTY);
    } else {
      BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validatorAddress, depositSize),
                        +Result::WITHDRAW_TOO_EARLY);
    }

    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);
  }

  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validatorAddress, depositSize), +Result::SUCCESS);
}

BOOST_AUTO_TEST_CASE(process_withdraw_completely_slashed) {

  FinalizationStateSpy spy;
  CAmount withdrawAmount = 0;
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(block_index);

  spy.CreateAndActivateDeposit(validatorAddress, depositSize);

  Validator *validator = &(*spy.pValidators())[validatorAddress];

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress), +Result::SUCCESS);
  spy.ProcessLogout(validatorAddress);

  // This is a double vote
  Vote v1 = {validatorAddress, uint256S("5"), 3, 5};
  Vote v2 = {validatorAddress, uint256S("6"), 3, 5};

  // Just to be sure we are after the lock period
  uint32_t endEpoch = spy.DynastyLogoutDelay() + spy.WithdrawalEpochDelay() + 10;

  for (uint32_t i = 6; i < endEpoch; ++i) {
    if (spy.GetCurrentDynasty() < validator->m_end_dynasty) {
      Vote vote{validatorAddress, targetHash, i - 2, i - 1};

      BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
      spy.ProcessVote(vote);
    }

    // Slash after a while
    if (i == 200) {
      BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SUCCESS);
      spy.ProcessSlash(v1, v2);
    }

    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);
  }

  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validatorAddress, withdrawAmount), +Result::SUCCESS);
}

BOOST_AUTO_TEST_SUITE_END()
