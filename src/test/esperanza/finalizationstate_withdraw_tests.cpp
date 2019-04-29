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

  finalization::Params params;
  FinalizationStateSpy spy(params);
  CAmount withdrawAmount = 0;

  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(RandValidatorAddr(), withdrawAmount),
                    +Result::WITHDRAW_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(process_withdraw_too_early) {

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

  // logout
  BOOST_CHECK_EQUAL(spy.ValidateLogout(validatorAddress), +Result::SUCCESS);
  spy.ProcessLogout(validatorAddress);
  BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 6);

  Validator *validator = &(*spy.pValidators())[validatorAddress];

  // Logout delay is set in dynasties but since we have finalization
  // every epoch, it's equal to number of epochs.
  // epoch=706 is the last epoch the finalizer can vote
  uint32_t end_logout = spy.GetCurrentEpoch() + static_cast<uint32_t>(spy.DynastyLogoutDelay());
  BOOST_CHECK_EQUAL(end_logout, 11);

  // From epoch end_logout+1 until end_withdraw-1 finalizer can't withdraw.
  // At end_withdraw or later finalizer can withdraw its deposit.
  uint32_t end_withdraw = end_logout + static_cast<uint32_t>(spy.WithdrawalEpochDelay()) + 1;
  BOOST_CHECK_EQUAL(end_withdraw, 22);

  for (uint32_t i = spy.GetCurrentEpoch(); i < end_withdraw; ++i) {
    if (spy.GetCurrentDynasty() <= validator->m_end_dynasty) {
      Vote vote{validatorAddress, targetHash, i - 2, i - 1};

      BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
      spy.ProcessVote(vote);
    }

    BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validatorAddress, depositSize), +Result::WITHDRAW_TOO_EARLY);
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);
  }

  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validatorAddress, depositSize), +Result::SUCCESS);
}

BOOST_AUTO_TEST_CASE(process_withdraw_completely_slashed) {

  finalization::Params params = finalization::Params::TestNet();
  FinalizationStateSpy spy(params);
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
