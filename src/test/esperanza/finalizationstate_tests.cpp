#include <esperanza/finalizationstate.h>
#include <keystore.h>
#include <random.h>
#include <test/test_unite.h>
#include <ufp64.h>
#include <util.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

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

using namespace esperanza;

const FinalizationParams params{};

class FinalizationStateSpy : public FinalizationState {
 public:
  FinalizationStateSpy() : FinalizationState(params) {}

  uint64_t* CurDynDeposits() { return &m_curDynDeposits; }
  uint64_t* PrevDynDeposits() { return &m_prevDynDeposits; }
  uint64_t* RewardFactor() { return &m_rewardFactor; }
  std::map<uint256, Validator>& Validators() { return m_validators; }
  std::map<uint256, Validator>* pValidators() { return &m_validators; }
  std::map<uint64_t, Checkpoint>& Checkpoints() { return m_checkpoints; }
  uint256* RecommendedTargetHash() { return &m_recommendedTargetHash; }

  int64_t EPOCH_LENGTH() const { return settings.m_epochLength; }
  CAmount MIN_DEPOSIT_SIZE() const { return settings.m_minDepositSize; }
  int64_t DYNASTY_LOGOUT_DELAY() const { return settings.m_dynastyLogoutDelay; }
  int64_t WITHDRAWAL_EPOCH_DELAY() const { return settings.m_withdrawalEpochDelay; }
  int64_t BOUNTY_FRACTION_DENOMINATOR() const { return settings.m_bountyFractionDenominator; }

  using FinalizationState::GetCurrentDynasty;
  using FinalizationState::GetDepositSize;
  using FinalizationState::InitializeEpoch;
  using FinalizationState::ProcessDeposit;
  using FinalizationState::ProcessLogout;
  using FinalizationState::ProcessSlash;
  using FinalizationState::ProcessVote;
  using FinalizationState::ProcessWithdraw;
};

BOOST_FIXTURE_TEST_SUITE(finalizationstate_tests, ReducedTestingSetup)

// Constructor tests

BOOST_AUTO_TEST_CASE(constructor) {

  FinalizationStateSpy state;

  BOOST_CHECK_EQUAL(0, state.GetCurrentEpoch());
  BOOST_CHECK_EQUAL(0, state.GetCurrentDynasty());
  BOOST_CHECK_EQUAL(0, state.GetLastFinalizedEpoch());
  BOOST_CHECK_EQUAL(0, state.GetLastJustifiedEpoch());
}

// InitializeEpoch tests

BOOST_AUTO_TEST_CASE(initialize_epcoh_wrong_height_passed) {

  FinalizationStateSpy state;

  BOOST_CHECK(state.InitializeEpoch(2 * state.EPOCH_LENGTH()) == +Result::INIT_WRONG_EPOCH);
  BOOST_CHECK(state.InitializeEpoch(state.EPOCH_LENGTH() - 1) == +Result::INIT_WRONG_EPOCH);
  BOOST_CHECK_EQUAL(0, state.GetCurrentEpoch());
  BOOST_CHECK_EQUAL(0, state.GetCurrentDynasty());
  BOOST_CHECK_EQUAL(0, state.GetLastFinalizedEpoch());
  BOOST_CHECK_EQUAL(0, state.GetLastJustifiedEpoch());
}

BOOST_AUTO_TEST_CASE(initialize_epcoh_insta_finalize) {

  FinalizationStateSpy spy;

  for (int i = 0; i < spy.EPOCH_LENGTH() * 3; i++) {
    if (i < spy.EPOCH_LENGTH()) {
      BOOST_CHECK(spy.InitializeEpoch(i) == +Result::INIT_WRONG_EPOCH);
    } else {
      if (i % spy.EPOCH_LENGTH() == 0) {
        BOOST_CHECK(spy.InitializeEpoch(i) == +Result::SUCCESS);
      }

      int expectedEpoch = i / spy.EPOCH_LENGTH();
      int expectedDynasty = (i / spy.EPOCH_LENGTH()) - 1;

      BOOST_CHECK_EQUAL(expectedEpoch, spy.GetCurrentEpoch());
      BOOST_CHECK_EQUAL(expectedDynasty, spy.GetCurrentDynasty());
      BOOST_CHECK_EQUAL(expectedDynasty, spy.GetLastFinalizedEpoch());
      BOOST_CHECK_EQUAL(expectedDynasty, spy.GetLastJustifiedEpoch());
    }
  }
}

// This tests assumes block time of 4s, hence epochs every 200s, and return of
// 6% per year given that the total deposit of validator is 150Mln units
BOOST_AUTO_TEST_CASE(initialize_epoch_reward_factor) {

  FinalizationStateSpy spy;
  *spy.CurDynDeposits() = 150000000;
  *spy.PrevDynDeposits() = 150000000;

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK_EQUAL("0.00057174", ufp64::to_str(*spy.RewardFactor()));
}

// Validate and ProcessDeposit tests

BOOST_AUTO_TEST_CASE(validate_deposit_tx_not_enough_deposit) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE() - 1;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::DEPOSIT_INSUFFICIENT);
}

BOOST_AUTO_TEST_CASE(validate_deposit_tx_double_deposit) {

  FinalizationStateSpy spy;

  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::DEPOSIT_ALREADY_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(process_deposit_tx) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  std::map<uint256, Validator> validators = spy.Validators();
  auto it = validators.find(validatorIndex);
  BOOST_CHECK(it != validators.end());

  Validator validator = it->second;
  BOOST_CHECK_EQUAL(validator.m_startDynasty, 2);  // assuming we start from 0
  BOOST_CHECK(validator.m_deposit > 0);
  BOOST_CHECK_EQUAL(it->first.GetHex(), validatorIndex.GetHex());
}

// ProcessVote and ValidateVote tests

BOOST_AUTO_TEST_CASE(validate_vote_tx_no_deposit) {

  FinalizationStateSpy spy;
  Vote vote{};

  BOOST_CHECK(spy.ValidateVote(vote) == +Result::VOTE_NOT_BY_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_too_early) {

  FinalizationStateSpy spy;

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // e0/d0 - a deposit is made
  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  // e0/d0 - try to vote but fail because too early
  Vote vote = {validatorIndex, targetHash, 0, 0};
  BOOST_CHECK(spy.ValidateVote(vote) == +Result::VOTE_NOT_VOTABLE);

  // e1/d0 - try to vote but fail because too early
  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  vote = {validatorIndex, targetHash, 0, 1};
  BOOST_CHECK(spy.ValidateVote(vote) == +Result::VOTE_NOT_VOTABLE);

  // e2/d1 - try to vote and succeed
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  vote = {validatorIndex, targetHash, 1, 2};
  BOOST_CHECK(spy.ValidateVote(vote) == +Result::VOTE_NOT_VOTABLE);

  // e3/d2 - try to vote and succeed
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  vote = {validatorIndex, targetHash, 2, 3};
  BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_already_voted) {

  FinalizationStateSpy spy;

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK(spy.InitializeEpoch(4 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(5 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(6 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  Vote vote = {validatorIndex, targetHash, 3, 6};

  BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
  spy.ProcessVote(vote);
  BOOST_CHECK(spy.ValidateVote(vote) == +Result::VOTE_ALREADY_VOTED);
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_wrong_target_epoch) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK(spy.InitializeEpoch(4 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(5 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(6 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  Vote vote = {validatorIndex, targetHash, 3, 5};

  BOOST_CHECK(spy.ValidateVote(vote) == +Result::VOTE_WRONG_TARGET_EPOCH);
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_wrong_target_hash) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  *spy.RecommendedTargetHash() = GetRandHash();

  uint256 targetHash = GetRandHash();

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK(spy.InitializeEpoch(4 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(5 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(6 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  Vote vote = {validatorIndex, targetHash, 3, 6};

  BOOST_CHECK(spy.ValidateVote(vote) == +Result::VOTE_WRONG_TARGET_HASH);
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_source_epoch_not_justified) {
  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK(spy.InitializeEpoch(4 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(5 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(6 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  Vote vote = {validatorIndex, targetHash, 4, 6};

  BOOST_CHECK(spy.ValidateVote(vote) == +Result::VOTE_SRC_EPOCH_NOT_JUSTIFIED);
}

BOOST_AUTO_TEST_CASE(process_vote_tx_success) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK(spy.InitializeEpoch(4 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(5 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  Vote vote = {validatorIndex, targetHash, 1, 5};

  BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
}

BOOST_AUTO_TEST_CASE(process_vote_tx_success_with_reward_no_consensus) {

  FinalizationStateSpy spy;
  uint256 validatorIndex_1 = GetRandHash();
  uint256 validatorIndex_2 = GetRandHash();
  CAmount depositSize_1 = spy.MIN_DEPOSIT_SIZE();
  CAmount depositSize_2 = spy.MIN_DEPOSIT_SIZE() * 2;

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex_1, depositSize_1) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex_1, depositSize_1);
  BOOST_CHECK(spy.ValidateDeposit(validatorIndex_2, depositSize_2) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex_2, depositSize_2);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK(spy.InitializeEpoch(4 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(5 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  Vote vote = {validatorIndex_1, targetHash, 3, 5};

  BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
  spy.ProcessVote(vote);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isJustified, false);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isFinalized, false);
}

BOOST_AUTO_TEST_CASE(process_vote_tx_success_with_finalization) {

  FinalizationStateSpy spy;
  uint256 validatorIndex_1 = GetRandHash();
  uint256 validatorIndex_2 = GetRandHash();
  CAmount depositSize_1 = spy.MIN_DEPOSIT_SIZE();
  CAmount depositSize_2 = spy.MIN_DEPOSIT_SIZE() * 3;

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex_1, depositSize_1) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex_1, depositSize_1);
  BOOST_CHECK(spy.ValidateDeposit(validatorIndex_2, depositSize_2) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex_2, depositSize_2);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  // The validator is included from here on
  BOOST_CHECK(spy.InitializeEpoch(4 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(5 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  Vote vote = {validatorIndex_2, targetHash, 3, 5};

  BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
  spy.ProcessVote(vote);

  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isJustified, true);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isFinalized, false);

  BOOST_CHECK(spy.InitializeEpoch(6 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;
  vote = {validatorIndex_2, targetHash, 5, 6};
  BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
  spy.ProcessVote(vote);

  BOOST_CHECK_EQUAL(spy.Checkpoints()[6].m_isJustified, true);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isFinalized, true);
}

// ProcessLogout and ValidateLogout tests

BOOST_AUTO_TEST_CASE(validate_logout_not_a_validator) {

  FinalizationStateSpy spy;

  BOOST_CHECK(spy.ValidateLogout(GetRandHash()) == +Result::LOGOUT_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(validate_logout_before_start_dynasty) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_CHECK(spy.ValidateLogout(validatorIndex) == +Result::LOGOUT_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(validate_logout_already_logged_out) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  BOOST_CHECK(spy.ValidateLogout(validatorIndex) == +Result::SUCCESS);
  spy.ProcessLogout(validatorIndex);

  BOOST_CHECK(spy.InitializeEpoch(4 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(5 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  BOOST_CHECK(spy.ValidateLogout(validatorIndex) == +Result::LOGOUT_ALREADY_DONE);
}

BOOST_AUTO_TEST_CASE(process_logout_end_dynasty) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  BOOST_CHECK(spy.ValidateLogout(validatorIndex) == +Result::SUCCESS);
  spy.ProcessLogout(validatorIndex);

  std::map<uint256, Validator> validators = spy.Validators();
  Validator validator = validators.find(validatorIndex)->second;
  BOOST_CHECK_EQUAL(702, validator.m_endDynasty);
}

// ProcessWithdraw and ValidateWithdraw tests

BOOST_AUTO_TEST_CASE(validate_withdraw_not_a_validator) {

  FinalizationStateSpy spy;
  CAmount withdrawAmount = 0;

  BOOST_CHECK(spy.ValidateWithdraw(GetRandHash(), withdrawAmount) == +Result::WITHDRAW_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(process_withdraw_before_end_dynasty) {

  FinalizationStateSpy spy;
  CAmount withdrawAmount = 0;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  BOOST_CHECK(spy.ValidateLogout(validatorIndex) == +Result::SUCCESS);
  spy.ProcessLogout(validatorIndex);

  for (int i = 4; i < spy.DYNASTY_LOGOUT_DELAY(); i++) {
    BOOST_CHECK(spy.InitializeEpoch(i * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
    Vote vote = {validatorIndex, targetHash, (uint32_t)i - 1, (uint32_t)i};

    BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
    spy.ProcessVote(vote);
  }

  BOOST_CHECK(spy.ValidateWithdraw(validatorIndex, withdrawAmount) == +Result::WITHDRAW_BEFORE_END_DYNASTY);
}

// epoch 2 - auto-finalized epochs make the validator now active
//
BOOST_AUTO_TEST_CASE(process_withdraw_too_early) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  // e0/d0 - create a deposit
  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  // e1/d0
  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  // e2/d1 - the validator is active
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  // e3/d2 - logout
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.ValidateLogout(validatorIndex) == +Result::SUCCESS);
  spy.ProcessLogout(validatorIndex);

  Validator* validator = &(*spy.pValidators())[validatorIndex];

  // The reason for this apparently magic "+ 4" is explained later on.
  int endEpoch = spy.DYNASTY_LOGOUT_DELAY() + spy.WITHDRAWAL_EPOCH_DELAY() + 4;

  int i = 4;
  for (; i <= endEpoch; i++) {
    BOOST_CHECK(spy.InitializeEpoch(i * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

    if (spy.GetCurrentDynasty() < validator->m_endDynasty) {
      Vote vote = {validatorIndex, targetHash, (uint32_t)i - 1, (uint32_t)i};

      BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
      spy.ProcessVote(vote);
    }
    // To explain why DYNASTY_LOGOUT_DELAY + 4 is correct the explaination is not trivial.
    // The end dynasty was set when we logged out (e3/d2) so it would be at least
    // DYNASTY_LOGOUT_DELAY + 3. Since we aim to reach finalization every epoch we
    // have that every epoch is finalized and hence a new dynasty is created exception
    // made for e(DYNASTY_LOGOUT_DELAY+2).
    // The reason for this is that since the function DepositExists() checks also the
    // previous dynasty deposits, in e(DYNASTY_LOGOUT_DELAY+2) we have the weird scenario
    // in which the only validator is logged out and cannot vote but his deposit still
    // counts to avoid InstaFinalize. Hence e(DYNASTY_LOGOUT_DELAY+2) cannot be finalized
    // and we need to wait for the next epoch to have finalization, hence
    // DYNASTY_LOGOUT_DELAY + 3 + 1.
    if( i <= spy.DYNASTY_LOGOUT_DELAY() + 4) {
      BOOST_CHECK(spy.ValidateWithdraw(validatorIndex, depositSize) == +Result::WITHDRAW_BEFORE_END_DYNASTY);
    } else {
      BOOST_CHECK(spy.ValidateWithdraw(validatorIndex, depositSize) == +Result::WITHDRAW_TOO_EARLY);
    }
  }

  BOOST_CHECK(spy.InitializeEpoch(i * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.ValidateWithdraw(validatorIndex, depositSize) == +Result::SUCCESS);
}

BOOST_AUTO_TEST_CASE(process_withdraw_completely_slashed) {

  FinalizationStateSpy spy;
  CAmount withdrawAmount = 0;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  Validator* validator = &(*spy.pValidators())[validatorIndex];

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  BOOST_CHECK(spy.ValidateLogout(validatorIndex) == +Result::SUCCESS);
  spy.ProcessLogout(validatorIndex);

  // This is a double vote
  Vote v1 = {validatorIndex, uint256S("5"), 3, 5};
  Vote v2 = {validatorIndex, uint256S("6"), 3, 5};
  CAmount bounty = 0;

  // Just to be sure we are after the lock period
  int endEpoch = spy.DYNASTY_LOGOUT_DELAY() + spy.WITHDRAWAL_EPOCH_DELAY() + 10;

  for (int i = 4; i < endEpoch; i++) {
    BOOST_CHECK(spy.InitializeEpoch(i * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

    if (spy.GetCurrentDynasty() < validator->m_endDynasty) {
      Vote vote = {validatorIndex, targetHash, (uint32_t)i - 1, (uint32_t)i};

      BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
      spy.ProcessVote(vote);
    }

    // Slash after a while
    if (i == 200) {
      BOOST_CHECK(spy.IsSlashable(v1, v2) == +Result::SUCCESS);
      spy.ProcessSlash(v1, v2, bounty);
    }
  }

  BOOST_CHECK(spy.ValidateWithdraw(validatorIndex, withdrawAmount) == +Result::SUCCESS);
}

// ProcessSlash and IsSlashable tests

BOOST_AUTO_TEST_CASE(is_slashable_not_a_validator) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();
  Vote v1 = {validatorIndex, uint256S("5"), 3, 5};
  Vote v2 = {validatorIndex, uint256S("15"), 3, 5};

  BOOST_CHECK(spy.IsSlashable(v1, v2) == +Result::SLASH_NOT_A_VALIDATOR);

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  v1 = {GetRandHash(), uint256S("5"), 3, 5};
  v2 = {validatorIndex, uint256S("15"), 3, 5};

  BOOST_CHECK(spy.IsSlashable(v1, v2) == +Result::SLASH_NOT_A_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(is_slashable_not_the_same_validator) {

  FinalizationStateSpy spy;
  uint256 validatorIndex_1 = GetRandHash();
  uint256 validatorIndex_2 = GetRandHash();
  CAmount depositSize_1 = spy.MIN_DEPOSIT_SIZE();
  CAmount depositSize_2 = spy.MIN_DEPOSIT_SIZE() + 1;

  Vote v1 = {validatorIndex_1, uint256S("5"), 3, 5};
  Vote v2 = {validatorIndex_2, uint256S("6"), 12, 52};

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex_1, depositSize_1) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex_1, depositSize_1);
  BOOST_CHECK(spy.ValidateDeposit(validatorIndex_2, depositSize_2) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex_2, depositSize_2);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.IsSlashable(v1, v2) == +Result::SLASH_NOT_SAME_VALIDATOR);
}

BOOST_AUTO_TEST_CASE(is_slashable_too_early) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  Vote v1 = {validatorIndex, uint256S("5"), 3, 5};
  Vote v2 = {validatorIndex, uint256S("6"), 12, 52};

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.IsSlashable(v2, v1) == +Result::SLASH_TOO_EARLY);
}

BOOST_AUTO_TEST_CASE(is_slashable_same_vote) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();
  Vote v1 = {validatorIndex, uint256S("5"), 3, 5};

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  int i;
  for (i = 4; i < 8; i++) {
    BOOST_CHECK(spy.InitializeEpoch(i * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

    Vote vote = {validatorIndex, targetHash, (uint32_t)i - 1, (uint32_t)i};

    BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
    spy.ProcessVote(vote);
  }

  BOOST_CHECK(spy.IsSlashable(v1, v1) == +Result::SLASH_SAME_VOTE);
}

BOOST_AUTO_TEST_CASE(is_slashable_already_slashed) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  Vote v1 = {validatorIndex, uint256S("5"), 3, 5};
  Vote v2 = {validatorIndex, uint256S("6"), 3, 5};
  CAmount bounty = 0;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  int i;
  for (i = 4; i < 8; i++) {
    BOOST_CHECK(spy.InitializeEpoch(i * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

    Vote vote = {validatorIndex, targetHash, (uint32_t)i - 1, (uint32_t)i};

    BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
    spy.ProcessVote(vote);
  }

  BOOST_CHECK(spy.IsSlashable(v1, v2) == +Result::SUCCESS);
  spy.ProcessSlash(v1, v2, bounty);

  BOOST_CHECK(spy.InitializeEpoch(i * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  BOOST_CHECK(spy.IsSlashable(v1, v2) == +Result::SLASH_ALREADY_SLASHED);
}

BOOST_AUTO_TEST_CASE(process_slash_duplicate_vote) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // This is a double vote
  Vote v1 = {validatorIndex, uint256S("5"), 3, 5};
  Vote v2 = {validatorIndex, uint256S("6"), 3, 5};
  CAmount bounty = 0;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  int i;
  for (i = 4; i < 8; i++) {
    BOOST_CHECK(spy.InitializeEpoch(i * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

    Vote vote = {validatorIndex, targetHash, (uint32_t)i - 1, (uint32_t)i};

    BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
    spy.ProcessVote(vote);
  }

  BOOST_CHECK(spy.IsSlashable(v1, v2) == +Result::SUCCESS);
  spy.ProcessSlash(v1, v2, bounty);

  CAmount totalDeposit = spy.GetDepositSize(validatorIndex);
  BOOST_CHECK_EQUAL(bounty, totalDeposit / spy.BOUNTY_FRACTION_DENOMINATOR());
}

BOOST_AUTO_TEST_CASE(process_slash_surrounding_vote) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // This is a surrounding
  Vote v1 = {validatorIndex, uint256S("5"), 1, 5};
  Vote v2 = {validatorIndex, uint256S("4"), 3, 4};
  CAmount bounty = 0;

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  int i;
  for (i = 4; i < 8; i++) {
    BOOST_CHECK(spy.InitializeEpoch(i * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

    Vote vote = {validatorIndex, targetHash, (uint32_t)i - 1, (uint32_t)i};

    BOOST_CHECK(spy.ValidateVote(vote) == +Result::SUCCESS);
    spy.ProcessVote(vote);
  }

  BOOST_CHECK(spy.IsSlashable(v1, v2) == +Result::SUCCESS);
  spy.ProcessSlash(v1, v2, bounty);

  CAmount totalDeposit = spy.GetDepositSize(validatorIndex);
  BOOST_CHECK_EQUAL(bounty, totalDeposit / spy.BOUNTY_FRACTION_DENOMINATOR());
}

// GetRecommendedVote tests
BOOST_AUTO_TEST_CASE(getrecommendedvote) {

  FinalizationStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  BOOST_CHECK(spy.ValidateDeposit(validatorIndex, depositSize) == +Result::SUCCESS);
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_CHECK(spy.InitializeEpoch(spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(2 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  BOOST_CHECK(spy.InitializeEpoch(3 * spy.EPOCH_LENGTH()) == +Result::SUCCESS);

  int i;
  for (i = 4; i < 8; i++) {
    BOOST_CHECK(spy.InitializeEpoch(i * spy.EPOCH_LENGTH()) == +Result::SUCCESS);
  }

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  Vote res = spy.GetRecommendedVote(validatorIndex);

  BOOST_CHECK_EQUAL(res.m_validatorIndex, validatorIndex);
  BOOST_CHECK_EQUAL(res.m_sourceEpoch, 3);
  BOOST_CHECK_EQUAL(res.m_targetEpoch, 7);
  BOOST_CHECK_EQUAL(res.m_targetHash, targetHash);
}

// Other tests

BOOST_AUTO_TEST_CASE(map_empty_initializer) {
  std::map<uint32_t, uint32_t> map;

  for (int i = 0; i < 100; i++) {
    BOOST_CHECK_EQUAL(0, map[i]);
  }
}

BOOST_AUTO_TEST_SUITE_END()
