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

BOOST_FIXTURE_TEST_SUITE(finalizationstate_withdraw_tests, ReducedTestingSetup)

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

  for (int i = 4; i < spy.DynastyLogoutDelay(); i++) {
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(i * spy.EpochLength()),
                      +Result::SUCCESS);
    Vote vote = {validatorAddress, targetHash, (uint32_t)i - 1, (uint32_t)i};

    BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
    spy.ProcessVote(vote);
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
  *spy.RecommendedTargetHash() = targetHash;

  // e0/d0 - create a deposit
  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);

  // e1/d0
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);

  // e2/d1 - the validator is active
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);

  // e3/d2 - logout
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress), +Result::SUCCESS);
  spy.ProcessLogout(validatorAddress);

  Validator *validator = &(*spy.pValidators())[validatorAddress];

  // The reason for this apparently magic "+ 4" is explained later on.
  int endEpoch = spy.DynastyLogoutDelay() + spy.WithdrawalEpochDelay() + 4;

  int i = 4;
  for (; i <= endEpoch; i++) {
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(i * spy.EpochLength()),
                      +Result::SUCCESS);

    if (spy.GetCurrentDynasty() < validator->m_endDynasty) {
      Vote vote = {validatorAddress, targetHash, (uint32_t)i - 1, (uint32_t)i};

      BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
      spy.ProcessVote(vote);
    }
    // To explain why DYNASTY_LOGOUT_DELAY + 4 is correct the explaination is
    // not trivial. The end dynasty was set when we logged out (e3/d2) so it
    // would be at least DYNASTY_LOGOUT_DELAY + 3. Since we aim to reach
    // finalization every epoch we have that every epoch is finalized and hence
    // a new dynasty is created exception made for e(DYNASTY_LOGOUT_DELAY+2).
    // The reason for this is that since the function DepositExists() checks
    // also the previous dynasty deposits, in e(DYNASTY_LOGOUT_DELAY+2) we have
    // the weird scenario in which the only validator is logged out and cannot
    // vote but his deposit still counts to avoid InstaFinalize. Hence
    // e(DYNASTY_LOGOUT_DELAY+2) cannot be finalized and we need to wait for the
    // next epoch to have finalization, hence DYNASTY_LOGOUT_DELAY + 3 + 1.
    if (i <= spy.DynastyLogoutDelay() + 4) {
      BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validatorAddress, depositSize),
                        +Result::WITHDRAW_BEFORE_END_DYNASTY);
    } else {
      BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validatorAddress, depositSize),
                        +Result::WITHDRAW_TOO_EARLY);
    }
  }

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(i * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validatorAddress, depositSize),
                    +Result::SUCCESS);
}

BOOST_AUTO_TEST_CASE(process_withdraw_completely_slashed) {

  FinalizationStateSpy spy;
  CAmount withdrawAmount = 0;
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);

  Validator *validator = &(*spy.pValidators())[validatorAddress];

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress), +Result::SUCCESS);
  spy.ProcessLogout(validatorAddress);

  // This is a double vote
  Vote v1 = {validatorAddress, uint256S("5"), 3, 5};
  Vote v2 = {validatorAddress, uint256S("6"), 3, 5};
  CAmount bounty = 0;

  // Just to be sure we are after the lock period
  int endEpoch = spy.DynastyLogoutDelay() + spy.WithdrawalEpochDelay() + 10;

  for (int i = 4; i < endEpoch; i++) {
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(i * spy.EpochLength()),
                      +Result::SUCCESS);

    if (spy.GetCurrentDynasty() < validator->m_endDynasty) {
      Vote vote = {validatorAddress, targetHash, (uint32_t)i - 1, (uint32_t)i};

      BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
      spy.ProcessVote(vote);
    }

    // Slash after a while
    if (i == 200) {
      BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SUCCESS);
      spy.ProcessSlash(v1, v2, bounty);
    }
  }

  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validatorAddress, withdrawAmount),
                    +Result::SUCCESS);
}

BOOST_AUTO_TEST_SUITE_END()
