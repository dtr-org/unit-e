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

BOOST_FIXTURE_TEST_SUITE(finalizationstate_vote_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(validate_vote_tx_no_deposit) {

  FinalizationStateSpy spy;
  Vote vote{};

  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::VOTE_NOT_BY_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_too_early) {

  FinalizationStateSpy spy;

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  block_index.nHeight = 0;
  spy.SetRecommendedTarget(&block_index);

  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // e0/d0 - a deposit is made
  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);

  // e0/d0 - try to vote but fail because wrong target
  Vote vote{validatorAddress, targetHash, 0, 0};
  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::VOTE_WRONG_TARGET_EPOCH);

  // e1/d0 - try to vote but fail because too early
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  vote = {validatorAddress, targetHash, 0, 0};
  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::VOTE_NOT_VOTABLE);

  // e2/d1 - try to vote but fail because too early
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  vote = {validatorAddress, targetHash, 0, 1};
  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::VOTE_NOT_VOTABLE);

  // e3/d1 - try to vote but fail because too early
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);
  vote = {validatorAddress, targetHash, 1, 2};
  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::VOTE_NOT_VOTABLE);

  // e4/d2 - try to vote and succeed
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(4 * spy.EpochLength()),
                    +Result::SUCCESS);
  vote = {validatorAddress, targetHash, 2, 3};
  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_already_voted) {

  FinalizationStateSpy spy;

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(&block_index);

  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(4 * spy.EpochLength()),
                    +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(5 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(6 * spy.EpochLength()),
                    +Result::SUCCESS);

  Vote vote{validatorAddress, targetHash, 3, 5};

  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
  spy.ProcessVote(vote);
  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::VOTE_ALREADY_VOTED);
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_wrong_target_epoch) {

  FinalizationStateSpy spy;
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(&block_index);

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(4 * spy.EpochLength()),
                    +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(5 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(6 * spy.EpochLength()),
                    +Result::SUCCESS);

  Vote vote{validatorAddress, targetHash, 3, 4};
  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::VOTE_WRONG_TARGET_EPOCH);

  vote = {validatorAddress, targetHash, 3, 6};
  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::VOTE_WRONG_TARGET_EPOCH);
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_wrong_target_hash) {

  FinalizationStateSpy spy;
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  uint256 old_target_hash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &old_target_hash;
  spy.SetRecommendedTarget(&block_index);

  uint256 targetHash = GetRandHash();

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(4 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(5 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(6 * spy.EpochLength()),
                    +Result::SUCCESS);

  Vote vote{validatorAddress, targetHash, 3, 6};

  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::VOTE_WRONG_TARGET_HASH);
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_source_epoch_not_justified) {
  FinalizationStateSpy spy;
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(&block_index);

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(4 * spy.EpochLength()),
                    +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(5 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(6 * spy.EpochLength()),
                    +Result::SUCCESS);

  Vote vote{validatorAddress, targetHash, 5, 5};

  BOOST_CHECK_EQUAL(spy.ValidateVote(vote),
                    +Result::VOTE_SRC_EPOCH_NOT_JUSTIFIED);
}

BOOST_AUTO_TEST_CASE(process_vote_tx_success) {

  FinalizationStateSpy spy;
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(&block_index);

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(4 * spy.EpochLength()),
                    +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(5 * spy.EpochLength()),
                    +Result::SUCCESS);

  Vote vote{validatorAddress, targetHash, 1, 4};

  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
}

BOOST_AUTO_TEST_CASE(process_vote_tx_success_with_reward_no_consensus) {

  FinalizationStateSpy spy;
  uint160 validatorAddress_1 = RandValidatorAddr();
  uint160 validatorAddress_2 = RandValidatorAddr();
  CAmount depositSize_1 = spy.MinDepositSize();
  CAmount depositSize_2 = spy.MinDepositSize() * 2;

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(&block_index);

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress_1, depositSize_1),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress_1, depositSize_1);
  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress_2, depositSize_2),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress_2, depositSize_2);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(4 * spy.EpochLength()),
                    +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(5 * spy.EpochLength()),
                    +Result::SUCCESS);

  Vote vote{validatorAddress_1, targetHash, 3, 4};

  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
  spy.ProcessVote(vote);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isJustified, false);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isFinalized, false);
}

BOOST_AUTO_TEST_CASE(process_vote_tx_success_with_finalization) {

  FinalizationStateSpy spy;
  uint160 validatorAddress_1 = RandValidatorAddr();
  uint160 validatorAddress_2 = RandValidatorAddr();
  CAmount depositSize_1 = spy.MinDepositSize();
  CAmount depositSize_2 = spy.MinDepositSize() * 3;

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  CBlockIndex block_index;
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(&block_index);

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress_1, depositSize_1),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress_1, depositSize_1);
  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress_2, depositSize_2),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress_2, depositSize_2);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(4 * spy.EpochLength()),
                    +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(5 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(6 * spy.EpochLength()),
                    +Result::SUCCESS);

  Vote vote{validatorAddress_2, targetHash, 4, 5};

  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
  spy.ProcessVote(vote);

  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isJustified, true);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isFinalized, false);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(7 * spy.EpochLength()),
                    +Result::SUCCESS);

  targetHash = GetRandHash();
  block_index.phashBlock = &targetHash;
  spy.SetRecommendedTarget(&block_index);
  vote = {validatorAddress_2, targetHash, 5, 6};
  BOOST_CHECK_EQUAL(spy.ValidateVote(vote), +Result::SUCCESS);
  spy.ProcessVote(vote);

  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isJustified, true);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[4].m_isFinalized, true);
}

BOOST_AUTO_TEST_SUITE_END()
