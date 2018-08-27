#include <esperanza/finalizationstate.h>
#include <keystore.h>
#include <random.h>
#include <test/test_unite.h>
#include <ufp64.h>
#include <util.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

const esperanza::FinalizationParams params{};

class EsperanzaStateSpy : public esperanza::FinalizationState {
 public:
  EsperanzaStateSpy() : FinalizationState(params) {}

  uint64_t* CurDynDeposits() { return &m_curDynDeposits; }
  uint64_t* PrevDynDeposits() { return &m_prevDynDeposits; }
  uint64_t* RewardFactor() { return &m_rewardFactor; }
  std::map<uint256, esperanza::Validator>& Validators() { return m_validators; }
  std::map<uint256, esperanza::Validator>* pValidators() {
    return &m_validators;
  }
  std::map<uint64_t, esperanza::Checkpoint>& Checkpoints() {
    return m_checkpoints;
  }
  uint256* RecommendedTargetHash() { return &m_recommendedTargetHash; }

  const int64_t EPOCH_LENGTH() { return esperanza::FinalizationState::EPOCH_LENGTH; }
  const CAmount MIN_DEPOSIT_SIZE() {
    return esperanza::FinalizationState::MIN_DEPOSIT_SIZE;
  }
  const int64_t DYNASTY_LOGOUT_DELAY() {
    return esperanza::FinalizationState::DYNASTY_LOGOUT_DELAY;
  }
  const int64_t WITHDRAWAL_EPOCH_DELAY() {
    return esperanza::FinalizationState::WITHDRAWAL_EPOCH_DELAY;
  }
  const int64_t BOUNTY_FRACTION_DENOMINATOR() {
    return esperanza::FinalizationState::BOUNTY_FRACTION_DENOMINATOR;
  }

  using esperanza::FinalizationState::GetCurrentDynasty;
  using esperanza::FinalizationState::GetDepositSize;
  using esperanza::FinalizationState::InitializeEpoch;
  using esperanza::FinalizationState::ProcessDeposit;
  using esperanza::FinalizationState::ProcessLogout;
  using esperanza::FinalizationState::ProcessSlash;
  using esperanza::FinalizationState::ProcessVote;
  using esperanza::FinalizationState::ProcessWithdraw;
};

BOOST_FIXTURE_TEST_SUITE(finalizationstate_tests, BasicTestingSetup)

// Constructor tests

BOOST_AUTO_TEST_CASE(constructor) {
  EsperanzaStateSpy state;

  BOOST_CHECK_EQUAL(0, state.GetCurrentEpoch());
  BOOST_CHECK_EQUAL(0, state.GetCurrentDynasty());
  BOOST_CHECK_EQUAL(0, state.GetLastFinalizedEpoch());
  BOOST_CHECK_EQUAL(0, state.GetLastJustifiedEpoch());
}

// InitializeEpoch tests

BOOST_AUTO_TEST_CASE(initialize_epcoh_wrong_height_passed) {
  EsperanzaStateSpy state;

  BOOST_TEST((state.InitializeEpoch(2 * state.EPOCH_LENGTH()) ==
              esperanza::Result::INIT_WRONG_EPOCH));
  BOOST_TEST((state.InitializeEpoch(state.EPOCH_LENGTH() - 1) ==
              esperanza::Result::INIT_WRONG_EPOCH));
  BOOST_CHECK_EQUAL(0, state.GetCurrentEpoch());
  BOOST_CHECK_EQUAL(0, state.GetCurrentDynasty());
  BOOST_CHECK_EQUAL(0, state.GetLastFinalizedEpoch());
  BOOST_CHECK_EQUAL(0, state.GetLastJustifiedEpoch());
}

BOOST_AUTO_TEST_CASE(initialize_epcoh_insta_finalize) {
  EsperanzaStateSpy spy;

  for (int i = 0; i < spy.EPOCH_LENGTH() * 3; i++) {
    if (i < spy.EPOCH_LENGTH()) {
      BOOST_TEST(
          (spy.InitializeEpoch(i) == esperanza::Result::INIT_WRONG_EPOCH));
    } else {
      if (i % spy.EPOCH_LENGTH() == 0) {
        BOOST_TEST((spy.InitializeEpoch(i) == esperanza::Result::SUCCESS));
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
  EsperanzaStateSpy spy;
  *spy.CurDynDeposits() = 150000000;
  *spy.PrevDynDeposits() = 150000000;

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_CHECK_EQUAL("0.00057174", ufp64::to_str(*spy.RewardFactor()));
}

// Validate and ProcessDeposit tests

BOOST_AUTO_TEST_CASE(validate_deposit_tx_not_enough_deposit) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE() - 1;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::DEPOSIT_INSUFFICIENT));
}

BOOST_AUTO_TEST_CASE(validate_deposit_tx_double_deposit) {
  EsperanzaStateSpy spy;

  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::DEPOSIT_ALREADY_VALIDATOR));
}

BOOST_AUTO_TEST_CASE(process_deposit_tx) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  std::map<uint256, esperanza::Validator> validators = spy.Validators();
  auto it = validators.find(validatorIndex);
  BOOST_TEST((it != validators.end()), "Validator not found.");

  esperanza::Validator validator = it->second;
  BOOST_CHECK_EQUAL(validator.m_startDynasty, 2);  // assuming we start from 0
  BOOST_TEST((validator.m_deposit > 0), "No deposit registered");
  BOOST_CHECK_EQUAL(it->first.GetHex(), validatorIndex.GetHex());
}

// ProcessVote and ValidateVote tests

BOOST_AUTO_TEST_CASE(validate_vote_tx_no_deposit) {
  EsperanzaStateSpy spy;
  esperanza::VoteData vote{};

  BOOST_TEST(
      (spy.ValidateVote(vote) == esperanza::Result::VOTE_NOT_A_VALIDATOR));
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_already_voted) {
  EsperanzaStateSpy spy;

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  // The validator is included from here on
  BOOST_TEST((spy.InitializeEpoch(4 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(5 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(6 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  esperanza::VoteData vote = {validatorIndex, targetHash, 3, 6};

  BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
  spy.ProcessVote(vote);
  BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::VOTE_ALREADY_VOTED));
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_wrong_target_epoch) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  // The validator is included from here on
  BOOST_TEST((spy.InitializeEpoch(4 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(5 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(6 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  esperanza::VoteData vote = {validatorIndex, targetHash, 3, 5};

  BOOST_TEST(
      (spy.ValidateVote(vote) == esperanza::Result::VOTE_WRONG_TARGET_EPOCH));
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_wrong_target_hash) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  *spy.RecommendedTargetHash() = GetRandHash();

  uint256 targetHash = GetRandHash();

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  // The validator is included from here on
  BOOST_TEST((spy.InitializeEpoch(4 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(5 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(6 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  esperanza::VoteData vote = {validatorIndex, targetHash, 3, 6};

  BOOST_TEST(
      (spy.ValidateVote(vote) == esperanza::Result::VOTE_WRONG_TARGET_HASH));
}

BOOST_AUTO_TEST_CASE(validate_vote_tx_non_votable_source_epoch_not_justified) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  // The validator is included from here on
  BOOST_TEST((spy.InitializeEpoch(4 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(5 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(6 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  esperanza::VoteData vote = {validatorIndex, targetHash, 4, 6};

  BOOST_TEST((spy.ValidateVote(vote) ==
              esperanza::Result::VOTE_SRC_EPOCH_NOT_JUSTIFIED));
}

BOOST_AUTO_TEST_CASE(process_vote_tx_success) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  // The validator is included from here on
  BOOST_TEST((spy.InitializeEpoch(4 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(5 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  esperanza::VoteData vote = {validatorIndex, targetHash, 1, 5};

  BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
}

BOOST_AUTO_TEST_CASE(process_vote_tx_success_with_reward_no_consensus) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex_1 = GetRandHash();
  uint256 validatorIndex_2 = GetRandHash();
  CAmount depositSize_1 = spy.MIN_DEPOSIT_SIZE();
  CAmount depositSize_2 = spy.MIN_DEPOSIT_SIZE() * 2;

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex_1, depositSize_1) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex_1, depositSize_1);
  BOOST_TEST((spy.ValidateDeposit(validatorIndex_2, depositSize_2) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex_2, depositSize_2);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  // The validator is included from here on
  BOOST_TEST((spy.InitializeEpoch(4 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(5 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  esperanza::VoteData vote = {validatorIndex_1, targetHash, 3, 5};

  BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
  spy.ProcessVote(vote);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isJustified, false);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isFinalized, false);
}

BOOST_AUTO_TEST_CASE(process_vote_tx_success_with_finalization) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex_1 = GetRandHash();
  uint256 validatorIndex_2 = GetRandHash();
  CAmount depositSize_1 = spy.MIN_DEPOSIT_SIZE();
  CAmount depositSize_2 = spy.MIN_DEPOSIT_SIZE() * 3;

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex_1, depositSize_1) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex_1, depositSize_1);
  BOOST_TEST((spy.ValidateDeposit(validatorIndex_2, depositSize_2) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex_2, depositSize_2);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  // The validator is included from here on
  BOOST_TEST((spy.InitializeEpoch(4 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(5 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  esperanza::VoteData vote = {validatorIndex_2, targetHash, 3, 5};

  BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
  spy.ProcessVote(vote);

  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isJustified, true);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isFinalized, false);

  BOOST_TEST((spy.InitializeEpoch(6 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;
  vote = {validatorIndex_2, targetHash, 5, 6};
  BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
  spy.ProcessVote(vote);

  BOOST_CHECK_EQUAL(spy.Checkpoints()[6].m_isJustified, true);
  BOOST_CHECK_EQUAL(spy.Checkpoints()[5].m_isFinalized, true);
}

// ProcessLogout and ValidateLogout tests

BOOST_AUTO_TEST_CASE(validate_logout_not_a_validator) {
  EsperanzaStateSpy spy;

  BOOST_TEST((spy.ValidateLogout(GetRandHash()) ==
              esperanza::Result::LOGOUT_NOT_A_VALIDATOR));
}

BOOST_AUTO_TEST_CASE(validate_logout_before_start_dynasty) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);
  BOOST_TEST((spy.ValidateLogout(validatorIndex) ==
              esperanza::Result::LOGOUT_NOT_A_VALIDATOR));
}

BOOST_AUTO_TEST_CASE(validate_logout_already_logged_out) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  BOOST_TEST(
      (spy.ValidateLogout(validatorIndex) == esperanza::Result::SUCCESS));
  spy.ProcessLogout(validatorIndex);

  BOOST_TEST((spy.InitializeEpoch(4 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(5 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  BOOST_TEST((spy.ValidateLogout(validatorIndex) ==
              esperanza::Result::LOGOUT_ALREADY_DONE));
}

BOOST_AUTO_TEST_CASE(process_logout_end_dynasty) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  BOOST_TEST(
      (spy.ValidateLogout(validatorIndex) == esperanza::Result::SUCCESS));
  spy.ProcessLogout(validatorIndex);

  std::map<uint256, esperanza::Validator> validators = spy.Validators();
  esperanza::Validator validator = validators.find(validatorIndex)->second;
  BOOST_CHECK_EQUAL(702, validator.m_endDynasty);
}

// ProcessWithdraw and ValidateWithdraw tests

BOOST_AUTO_TEST_CASE(validate_withdraw_not_a_validator) {
  EsperanzaStateSpy spy;
  CAmount withdrawAmount = 0;

  BOOST_TEST((spy.ValidateWithdraw(GetRandHash(), withdrawAmount) ==
              esperanza::Result::WITHDRAW_NOT_A_VALIDATOR));
}

BOOST_AUTO_TEST_CASE(process_withdraw_before_end_dynasty) {
  EsperanzaStateSpy spy;
  CAmount withdrawAmount = 0;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  BOOST_TEST(
      (spy.ValidateLogout(validatorIndex) == esperanza::Result::SUCCESS));
  spy.ProcessLogout(validatorIndex);

  for (int i = 4; i < spy.DYNASTY_LOGOUT_DELAY(); i++) {
    BOOST_TEST((spy.InitializeEpoch(i * spy.EPOCH_LENGTH() + 1) ==
                esperanza::Result::SUCCESS));
    esperanza::VoteData vote = {validatorIndex, targetHash, (uint32_t)i - 1,
                                (uint32_t)i};

    BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
    spy.ProcessVote(vote);
  }

  BOOST_TEST((spy.ValidateWithdraw(validatorIndex, withdrawAmount) ==
              esperanza::Result::WITHDRAW_BEFORE_END_DYNASTY));
}

BOOST_AUTO_TEST_CASE(process_withdraw_too_early) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  BOOST_TEST(
      (spy.ValidateLogout(validatorIndex) == esperanza::Result::SUCCESS));
  spy.ProcessLogout(validatorIndex);

  esperanza::Validator* validator = &(*spy.pValidators())[validatorIndex];

  for (int i = 4; i < spy.WITHDRAWAL_EPOCH_DELAY(); i++) {
    BOOST_TEST((spy.InitializeEpoch(i * spy.EPOCH_LENGTH() + 1) ==
                esperanza::Result::SUCCESS));

    if (spy.GetCurrentDynasty() < validator->m_endDynasty) {
      esperanza::VoteData vote = {validatorIndex, targetHash, (uint32_t)i - 1,
                                  (uint32_t)i};

      BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
      spy.ProcessVote(vote);
    }
  }

  BOOST_TEST((spy.ValidateWithdraw(validatorIndex, depositSize) ==
              esperanza::Result::WITHDRAW_TOO_EARLY));
}

BOOST_AUTO_TEST_CASE(process_withdraw_completely_slashed) {
  EsperanzaStateSpy spy;
  CAmount withdrawAmount = 0;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  esperanza::Validator* validator = &(*spy.pValidators())[validatorIndex];

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  BOOST_TEST(
      (spy.ValidateLogout(validatorIndex) == esperanza::Result::SUCCESS));
  spy.ProcessLogout(validatorIndex);

  // This is a double vote
  esperanza::VoteData v1 = {validatorIndex, uint256S("5"), 3, 5};
  esperanza::VoteData v2 = {validatorIndex, uint256S("6"), 3, 5};
  CAmount bounty = 0;

  int endEpoch = spy.DYNASTY_LOGOUT_DELAY() + spy.WITHDRAWAL_EPOCH_DELAY() +
                 10;  // Just to be sure we are after the lock period

  for (int i = 4; i < endEpoch; i++) {
    BOOST_TEST((spy.InitializeEpoch(i * spy.EPOCH_LENGTH() + 1) ==
                esperanza::Result::SUCCESS));

    if (spy.GetCurrentDynasty() < validator->m_endDynasty) {
      esperanza::VoteData vote = {validatorIndex, targetHash, (uint32_t)i - 1,
                                  (uint32_t)i};

      BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
      spy.ProcessVote(vote);
    }

    // Slash after a while
    if (i == 200) {
      BOOST_TEST((spy.IsSlashable(v1, v2) == esperanza::Result::SUCCESS));
      spy.ProcessSlash(v1, v2, bounty);
    }
  }

  BOOST_TEST((spy.ValidateWithdraw(validatorIndex, withdrawAmount) ==
              esperanza::Result::SUCCESS));
}

// ProcessSlash and IsSlashable tests

BOOST_AUTO_TEST_CASE(is_slashable_not_a_validator) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();
  esperanza::VoteData v1 = {validatorIndex, uint256S("5"), 3, 5};
  esperanza::VoteData v2 = {validatorIndex, uint256S("15"), 3, 5};

  BOOST_TEST(
      (spy.IsSlashable(v1, v2) == esperanza::Result::SLASH_NOT_A_VALIDATOR));

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  v1 = {GetRandHash(), uint256S("5"), 3, 5};
  v2 = {validatorIndex, uint256S("15"), 3, 5};

  BOOST_TEST(
      (spy.IsSlashable(v1, v2) == esperanza::Result::SLASH_NOT_A_VALIDATOR));
}

BOOST_AUTO_TEST_CASE(is_slashable_not_the_same_validator) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex_1 = GetRandHash();
  uint256 validatorIndex_2 = GetRandHash();
  CAmount depositSize_1 = spy.MIN_DEPOSIT_SIZE();
  CAmount depositSize_2 = spy.MIN_DEPOSIT_SIZE() + 1;

  esperanza::VoteData v1 = {validatorIndex_1, uint256S("5"), 3, 5};
  esperanza::VoteData v2 = {validatorIndex_2, uint256S("6"), 12, 52};
  CAmount bounty = 0;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex_1, depositSize_1) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex_1, depositSize_1);
  BOOST_TEST((spy.ValidateDeposit(validatorIndex_2, depositSize_2) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex_2, depositSize_2);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST(
      (spy.IsSlashable(v1, v2) == esperanza::Result::SLASH_NOT_SAME_VALIDATOR));
}

BOOST_AUTO_TEST_CASE(is_slashable_too_early) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  esperanza::VoteData v1 = {validatorIndex, uint256S("5"), 3, 5};
  esperanza::VoteData v2 = {validatorIndex, uint256S("6"), 12, 52};

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.IsSlashable(v2, v1) == esperanza::Result::SLASH_TOO_EARLY));
}

BOOST_AUTO_TEST_CASE(is_slashable_same_vote) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();
  esperanza::VoteData v1 = {validatorIndex, uint256S("5"), 3, 5};
  CAmount bounty = 0;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;
  CAmount awardedDeposit;

  int i;
  for (i = 4; i < 8; i++) {
    BOOST_TEST((spy.InitializeEpoch(i * spy.EPOCH_LENGTH() + 1) ==
                esperanza::Result::SUCCESS));

    esperanza::VoteData vote = {validatorIndex, targetHash, (uint32_t)i - 1,
                                (uint32_t)i};

    BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
    spy.ProcessVote(vote);
  }

  BOOST_TEST((spy.IsSlashable(v1, v1) == esperanza::Result::SLASH_SAME_VOTE));
}

BOOST_AUTO_TEST_CASE(is_slashable_already_slashed) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  esperanza::VoteData v1 = {validatorIndex, uint256S("5"), 3, 5};
  esperanza::VoteData v2 = {validatorIndex, uint256S("6"), 3, 5};
  CAmount bounty = 0;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  int i;
  for (i = 4; i < 8; i++) {
    BOOST_TEST((spy.InitializeEpoch(i * spy.EPOCH_LENGTH() + 1) ==
                esperanza::Result::SUCCESS));

    esperanza::VoteData vote = {validatorIndex, targetHash, (uint32_t)i - 1,
                                (uint32_t)i};

    BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
    spy.ProcessVote(vote);
  }

  BOOST_TEST((spy.IsSlashable(v1, v2) == esperanza::Result::SUCCESS));
  spy.ProcessSlash(v1, v2, bounty);

  BOOST_TEST((spy.InitializeEpoch(i * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  BOOST_TEST(
      (spy.IsSlashable(v1, v2) == esperanza::Result::SLASH_ALREADY_SLASHED));
}

BOOST_AUTO_TEST_CASE(process_slash_duplicate_vote) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // This is a double vote
  esperanza::VoteData v1 = {validatorIndex, uint256S("5"), 3, 5};
  esperanza::VoteData v2 = {validatorIndex, uint256S("6"), 3, 5};
  CAmount bounty = 0;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;
  CAmount awardedDeposit;

  int i;
  for (i = 4; i < 8; i++) {
    BOOST_TEST((spy.InitializeEpoch(i * spy.EPOCH_LENGTH() + 1) ==
                esperanza::Result::SUCCESS));

    esperanza::VoteData vote = {validatorIndex, targetHash, (uint32_t)i - 1,
                                (uint32_t)i};

    BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
    spy.ProcessVote(vote);
  }

  BOOST_TEST((spy.IsSlashable(v1, v2) == esperanza::Result::SUCCESS));
  spy.ProcessSlash(v1, v2, bounty);

  CAmount totalDeposit = spy.GetDepositSize(validatorIndex);
  BOOST_CHECK_EQUAL(bounty, totalDeposit / spy.BOUNTY_FRACTION_DENOMINATOR());
}

BOOST_AUTO_TEST_CASE(process_slash_surrounding_vote) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  // This is a surrounding
  esperanza::VoteData v1 = {validatorIndex, uint256S("5"), 1, 5};
  esperanza::VoteData v2 = {validatorIndex, uint256S("4"), 3, 4};
  CAmount bounty = 0;

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  int i;
  for (i = 4; i < 8; i++) {
    BOOST_TEST((spy.InitializeEpoch(i * spy.EPOCH_LENGTH() + 1) ==
                esperanza::Result::SUCCESS));

    esperanza::VoteData vote = {validatorIndex, targetHash, (uint32_t)i - 1,
                                (uint32_t)i};

    BOOST_TEST((spy.ValidateVote(vote) == esperanza::Result::SUCCESS));
    spy.ProcessVote(vote);
  }

  BOOST_TEST((spy.IsSlashable(v1, v2) == esperanza::Result::SUCCESS));
  spy.ProcessSlash(v1, v2, bounty);

  CAmount totalDeposit = spy.GetDepositSize(validatorIndex);
  BOOST_CHECK_EQUAL(bounty, totalDeposit / spy.BOUNTY_FRACTION_DENOMINATOR());
}

// GetRecommendedVote tests
BOOST_AUTO_TEST_CASE(getrecommendedvote) {
  EsperanzaStateSpy spy;
  uint256 validatorIndex = GetRandHash();
  CAmount depositSize = spy.MIN_DEPOSIT_SIZE();

  BOOST_TEST((spy.ValidateDeposit(validatorIndex, depositSize) ==
              esperanza::Result::SUCCESS));
  spy.ProcessDeposit(validatorIndex, depositSize);

  BOOST_TEST((spy.InitializeEpoch(spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(2 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));
  BOOST_TEST((spy.InitializeEpoch(3 * spy.EPOCH_LENGTH() + 1) ==
              esperanza::Result::SUCCESS));

  int i;
  for (i = 4; i < 8; i++) {
    BOOST_TEST((spy.InitializeEpoch(i * spy.EPOCH_LENGTH() + 1) ==
                esperanza::Result::SUCCESS));
  }

  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  esperanza::VoteData res = spy.GetRecommendedVote(validatorIndex);

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
