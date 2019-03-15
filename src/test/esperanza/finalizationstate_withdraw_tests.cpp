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
  CAmount withdraw_amount = 0;

  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(RandValidatorAddr(), withdraw_amount),
                    +Result::WITHDRAW_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(process_withdraw_before_end_dynasty) {

  FinalizationStateSpy spy;
  CAmount withdraw_amount = 0;
  uint160 validator_address = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the target_hash constant since it does not
  // affect the state.
  uint256 target_hash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &target_hash;
  spy.SetRecommendedTarget(block_index);

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, depositSize), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, depositSize);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 1 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 2 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 3 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 4 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetActiveFinalizers().size(), 1);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validator_address), +Result::SUCCESS);
  spy.ProcessLogout(validator_address);

  for (uint32_t i = 5; i < spy.DynastyLogoutDelay() + 1; ++i) {
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);
    Vote vote{validator_address, target_hash, i - 1, i};

    BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
    spy.ProcessVote(vote);
  }

  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validator_address, withdraw_amount),
                    +Result::WITHDRAW_BEFORE_END_DYNASTY);
}

BOOST_AUTO_TEST_CASE(process_withdraw_too_early) {

  FinalizationStateSpy spy;
  uint160 validator_address = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the target_hash constant since it does not
  // affect the state.
  uint256 target_hash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &target_hash;
  spy.SetRecommendedTarget(block_index);

  // e1/d0 - create a deposit
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, depositSize), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, depositSize);
  BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 1);
  BOOST_CHECK_EQUAL(spy.GetCurrentDynasty(), 0);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 1 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 2);
  BOOST_CHECK_EQUAL(spy.GetCurrentDynasty(), 0);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 2 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 3);
  BOOST_CHECK_EQUAL(spy.GetCurrentDynasty(), 1);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 3 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 4);
  BOOST_CHECK_EQUAL(spy.GetCurrentDynasty(), 2);
  BOOST_CHECK_EQUAL(spy.GetActiveFinalizers().size(), 0);

  // the validator is active
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 4 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 5);
  BOOST_CHECK_EQUAL(spy.GetCurrentDynasty(), 3);
  BOOST_CHECK_EQUAL(spy.GetActiveFinalizers().size(), 1);

  // logout
  BOOST_CHECK_EQUAL(spy.ValidateLogout(validator_address), +Result::SUCCESS);
  spy.ProcessLogout(validator_address);

  Validator *validator = &(*spy.pValidators())[validator_address];

  // The reason for this apparently magic "+ 3" is explained later on.
  uint32_t end_epoch = spy.DynastyLogoutDelay() + spy.WithdrawalEpochDelay() + 3;

  uint32_t i = 5;
  for (; i <= end_epoch; ++i) {
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);

    if (spy.GetCurrentDynasty() < validator->m_end_dynasty) {
      Vote vote{validator_address, target_hash, i - 1, i};

      BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
      spy.ProcessVote(vote);
    }
    // To explain why DYNASTY_LOGOUT_DELAY + 3 is correct the explanation is
    // not trivial. The end dynasty was set when we logged out (e5/d3) so it
    // would be at least DYNASTY_LOGOUT_DELAY + 3. Since we aim to reach
    // finalization every epoch we have that every epoch is finalized and hence
    // a new dynasty is created exception made for e(DYNASTY_LOGOUT_DELAY+2).
    // The reason for this is that since the function DepositExists() checks
    // also the previous dynasty deposits, in e(DYNASTY_LOGOUT_DELAY+2) we have
    // the weird scenario in which the only validator is logged out and cannot
    // vote but his deposit still counts to avoid InstaJustify. Hence
    // e(DYNASTY_LOGOUT_DELAY+2) cannot be finalized and we need to wait for the
    // next epoch to have finalization, hence DYNASTY_LOGOUT_DELAY + 3 + 1.
    if (i <= spy.DynastyLogoutDelay() + 4) {
      BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validator_address, depositSize),
                        +Result::WITHDRAW_BEFORE_END_DYNASTY);
    } else {
      BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validator_address, depositSize),
                        +Result::WITHDRAW_TOO_EARLY);
    }
  }

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validator_address, depositSize), +Result::SUCCESS);
}

BOOST_AUTO_TEST_CASE(process_withdraw_completely_slashed) {

  FinalizationStateSpy spy;
  CAmount withdraw_amount = 0;
  uint160 validator_address = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the target_hash constant since it does not
  // affect the state.
  uint256 target_hash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &target_hash;
  spy.SetRecommendedTarget(block_index);

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, depositSize), +Result::SUCCESS);
  spy.ProcessDeposit(validator_address, depositSize);

  Validator *validator = &(*spy.pValidators())[validator_address];

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 1 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 2 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 3 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + 4 * spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetActiveFinalizers().size(), 1);

  BOOST_CHECK_EQUAL(spy.ValidateLogout(validator_address), +Result::SUCCESS);
  spy.ProcessLogout(validator_address);

  // This is a double vote
  Vote v1 = {validator_address, uint256S("5"), 3, 5};
  Vote v2 = {validator_address, uint256S("6"), 3, 5};

  // Just to be sure we are after the lock period
  uint32_t end_epoch = spy.DynastyLogoutDelay() + spy.WithdrawalEpochDelay() + 10;

  for (uint32_t i = 5; i < end_epoch; ++i) {
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(1 + i * spy.EpochLength()), +Result::SUCCESS);

    if (spy.GetCurrentDynasty() < validator->m_end_dynasty) {
      Vote vote{validator_address, target_hash, i - 1, i};

      BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
      spy.ProcessVote(vote);
    }

    // Slash after a while
    if (i == 200) {
      BOOST_CHECK_EQUAL(spy.IsSlashable(v1, v2), +Result::SUCCESS);
      spy.ProcessSlash(v1, v2);
    }
  }

  BOOST_CHECK_EQUAL(spy.ValidateWithdraw(validator_address, withdraw_amount), +Result::SUCCESS);
}

BOOST_AUTO_TEST_SUITE_END()
