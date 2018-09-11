// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <esperanza/finalizationstate.h>
#include <esperanza/vote.h>
#include <script/ismine.h>
#include <stdio.h>
#include <tinyformat.h>
#include <ufp64.h>
#include <util.h>
#include <validation.h>
#include <utility>

namespace esperanza {

/**
 * UNIT-E: It is now simplistic to imply only one State; to be able to
 * handle intra-dynasty forks and continue with a consistent state it's gonna be
 * necessary to store a snapshot of the state for each block from at least the
 * last justified one.
 */
static std::shared_ptr<FinalizationState> esperanzaState;

static CCriticalSection cs_init_lock;

const ufp64::ufp64_t BASE_DEPOSIT_SCALE_FACTOR = ufp64::to_ufp64(1);

template <typename... Args>
inline Result fail(Result error, const char *fmt, const Args &... args) {
  std::string reason = tfm::format(fmt, args...);
  LogPrintStr("ERROR: ESPERANZA - " + reason + "\n");
  return error;
}

Result success() { return Result::SUCCESS; }

FinalizationState::FinalizationState(
    const esperanza::FinalizationParams &params)
    : m_mainHashJustified(false),
      m_currentEpoch(0),
      m_currentDynasty(0),
      m_curDynDeposits(0),
      m_prevDynDeposits(0),
      m_expectedSrcEpoch(0),
      m_lastFinalizedEpoch(0),
      m_lastJustifiedEpoch(0),
      m_lastVoterRescale(0),
      m_lastNonVoterRescale(0),
      m_rewardFactor(0),
      EPOCH_LENGTH(params.m_epochLength),
      MIN_DEPOSIT_SIZE(params.m_minDepositSize),
      DYNASTY_LOGOUT_DELAY(params.m_dynastyLogoutDelay),
      WITHDRAWAL_EPOCH_DELAY(params.m_withdrawalEpochDelay),
      SLASH_FRACTION_MULTIPLIER(params.m_slashFractionMultiplier),
      BOUNTY_FRACTION_DENOMINATOR(params.m_bountyFractionDenominator),
      BASE_INTEREST_FACTOR(params.m_baseInterestFactor),
      BASE_PENALTY_FACTOR(params.m_basePenaltyFactor) {
  m_depositScaleFactor[0] = BASE_DEPOSIT_SCALE_FACTOR;
  m_checkpoints[0] = Checkpoint{};
  m_checkpoints[0].m_isJustified = true;
  m_checkpoints[0].m_isFinalized = true;
}

/**
 * If the block height passed is the first of a new epoch, then we prepare the
 * new epoch.
 * @param blockHeight the block height.
 */
esperanza::Result FinalizationState::InitializeEpoch(int blockHeight) {
  LOCK(cs_esperanza);

  auto newEpoch = static_cast<uint32_t>(blockHeight) / EPOCH_LENGTH;

  if (newEpoch != m_currentEpoch + 1) {
    return fail(esperanza::Result::INIT_WRONG_EPOCH,
                "%s: Next epoch should be %d but %d was passed.\n", __func__,
                m_currentEpoch + 1, newEpoch);
  }

  LogPrint(BCLog::ESPERANZA, "%s: New epoch found, this epoch is the %d.\n",
           __func__, newEpoch);

  m_checkpoints[newEpoch] = Checkpoint{};
  m_checkpoints[newEpoch].m_curDynastyDeposits = GetTotalCurDynDeposits();
  m_checkpoints[newEpoch].m_prevDynastyDeposits = GetTotalPrevDynDeposits();

  m_currentEpoch = newEpoch;

  LogPrint(BCLog::ESPERANZA, "%s: Epoch block found at height %d.\n",
           __func__, blockHeight);

  m_lastVoterRescale = ufp64::add_uint(GetCollectiveRewardFactor(), 1);

  m_lastNonVoterRescale =
      ufp64::div(m_lastVoterRescale, (ufp64::add_uint(m_rewardFactor, 1)));

  m_depositScaleFactor[newEpoch] =
      ufp64::mul(m_lastNonVoterRescale, m_depositScaleFactor[newEpoch - 1]);

  m_totalSlashed[newEpoch] = m_totalSlashed[newEpoch - 1];

  if (DepositExists()) {
    ufp64::ufp64_t interestBase =
        ufp64::div(BASE_INTEREST_FACTOR, GetSqrtOfTotalDeposits());

    m_rewardFactor = ufp64::add(
        interestBase,
        ufp64::mul_by_uint(BASE_PENALTY_FACTOR, GetEpochsSinceFinalization()));

    if (m_rewardFactor <= 0) {
      return fail(esperanza::Result::INIT_INVALID_REWARD,
                  "Invalid reward factor %d", m_rewardFactor);
    }

  } else {
    InstaFinalize();
    m_rewardFactor = 0;
  }

  m_epochToCheckpointHash[m_currentEpoch] = m_recommendedTargetHash;

  IncrementDynasty();

  LogPrint(BCLog::ESPERANZA,
           "%s: Epoch with hash %s and height %d initialized.\n", __func__,
           m_recommendedTargetHash.GetHex(), newEpoch);

  return success();
}

/**
 * In case there is nobody available to finalize we finalize automatically.
 */
void FinalizationState::InstaFinalize() {
  uint32_t epoch = this->m_currentEpoch;
  m_mainHashJustified = true;
  m_checkpoints[epoch - 1].m_isJustified = true;
  m_checkpoints[epoch - 1].m_isFinalized = true;
  m_lastJustifiedEpoch = epoch - 1;
  m_lastFinalizedEpoch = epoch - 1;

  LogPrint(BCLog::ESPERANZA, "%s: Finalized block for epoch %d.\n", __func__,
           epoch);
}

/**
 * Increments the current dynasty if finalization has been reached.
 */
void FinalizationState::IncrementDynasty() {
  uint32_t epoch = this->m_currentEpoch;

  if (m_checkpoints[epoch - 2].m_isFinalized) {
    LogPrint(BCLog::ESPERANZA, "%s: Epoch %d is finalized.\n", __func__,
             epoch - 2);

    m_currentDynasty += 1;
    m_prevDynDeposits = m_curDynDeposits;
    m_curDynDeposits += m_dynastyDeltas[m_currentDynasty];
    m_dynastyStartEpoch[m_currentDynasty] = epoch;
    // UNIT-E: we can clear old checkpoints (up to lastFinalizedEpoch - 1)
  }
  m_epochToDynasty[epoch] = m_currentDynasty;

  if (m_mainHashJustified) {
    m_expectedSrcEpoch = epoch - 1;
    m_mainHashJustified = false;
  }
}

ufp64::ufp64_t FinalizationState::GetCollectiveRewardFactor() {
  uint32_t epoch = m_currentEpoch;
  bool isLive = GetEpochsSinceFinalization() <= 2;

  if (!DepositExists() || !isLive) {
    return 0;
  }

  ufp64::ufp64_t curVoteFraction = ufp64::div_2uint(
      m_checkpoints[epoch - 1].m_curDynastyVotes[m_expectedSrcEpoch],
      m_curDynDeposits);

  ufp64::ufp64_t prevVoteFraction = ufp64::div_2uint(
      m_checkpoints[epoch - 1].m_prevDynastyVotes[m_expectedSrcEpoch],
      m_prevDynDeposits);

  ufp64::ufp64_t voteFraction = ufp64::min(curVoteFraction, prevVoteFraction);

  return ufp64::div_by_uint(ufp64::mul(voteFraction, m_rewardFactor), 2);
}

/**
 * @return true if any deposit exists, false otherwise.
 */
bool FinalizationState::DepositExists() {
  return m_curDynDeposits > 0 && m_prevDynDeposits > 0;
}

ufp64::ufp64_t FinalizationState::GetSqrtOfTotalDeposits() {
  uint64_t totalDeposits =
      1 + ufp64::mul_to_uint(m_depositScaleFactor[m_currentEpoch - 1],
                             std::max(m_prevDynDeposits, m_curDynDeposits));

  return ufp64::sqrt_uint(totalDeposits);
}

/**
 * @return epochs since the last finalization.
 */
uint32_t FinalizationState::GetEpochsSinceFinalization() {
  return m_currentEpoch - m_lastFinalizedEpoch;
}

/**
 * Removes a validator from the validator list.
 * @param validatorIndex the index of the validator to remove.
 */
void FinalizationState::DeleteValidator(const uint256 &validatorIndex) {
  LOCK(cs_esperanza);

  m_validators.erase(validatorIndex);
}

uint64_t FinalizationState::GetDepositSize(
    const uint256 &validatorIndex) const {
  LOCK(cs_esperanza);

  auto validatorIt = m_validators.find(validatorIndex);
  auto depositScaleIt = m_depositScaleFactor.find(m_currentEpoch);

  if (validatorIt != m_validators.end() &&
      depositScaleIt != m_depositScaleFactor.end()) {
    return ufp64::mul_to_uint(depositScaleIt->second,
                              validatorIt->second.m_deposit);
  } else {
    return 0;
  }
}

Vote FinalizationState::GetRecommendedVote(
    const uint256 &validatorIndex) const {
  LOCK(cs_esperanza);

  Vote vote;
  vote.m_validatorIndex = validatorIndex;
  vote.m_targetHash = m_recommendedTargetHash;
  vote.m_targetEpoch = m_currentEpoch;
  vote.m_sourceEpoch = m_expectedSrcEpoch;

  LogPrint(BCLog::ESPERANZA,
           "%s: Getting recommended vote for epoch %d and dynasty %d is: { %s, "
           "%d, %d }.\n",
           __func__, m_currentEpoch, m_currentDynasty,
           m_recommendedTargetHash.GetHex(), m_currentEpoch,
           m_expectedSrcEpoch);

  return vote;
}

bool FinalizationState::IsInDynasty(const Validator &validator,
                                    uint32_t dynasty) const {
  uint32_t startDynasty = validator.m_startDynasty;
  uint32_t endDynasty = validator.m_endDynasty;
  return (startDynasty <= dynasty) && (dynasty < endDynasty);
}

uint64_t FinalizationState::GetTotalCurDynDeposits() {
  return ufp64::mul_to_uint(m_depositScaleFactor[m_currentEpoch],
                            m_curDynDeposits);
}

uint64_t FinalizationState::GetTotalPrevDynDeposits() {
  return ufp64::mul_to_uint(m_depositScaleFactor[m_currentEpoch - 1],
                            m_prevDynDeposits);
}

CAmount FinalizationState::ProcessReward(const uint256 &validatorIndex,
                                         uint64_t reward) {
  m_validators[validatorIndex].m_deposit += reward;
  uint32_t startDynasty = m_validators[validatorIndex].m_startDynasty;
  uint32_t endDynasty = m_validators[validatorIndex].m_endDynasty;

  if ((startDynasty <= m_currentDynasty) && (m_currentDynasty < endDynasty)) {
    m_curDynDeposits += reward;
  }

  if ((startDynasty <= m_currentDynasty - 1) &&
      (m_currentDynasty - 1 < endDynasty)) {
    m_prevDynDeposits += reward;
  }

  if (endDynasty < DEFAULT_END_DYNASTY) {
    m_dynastyDeltas[endDynasty] -= reward;
  }

  return ufp64::mul_to_uint(m_depositScaleFactor[m_currentEpoch],
                            m_validators[validatorIndex].m_deposit);

  // UNIT-E: Here is where we should reward proposers if we want
}

/**
 * Check whether the input provided makes a valid vote.
 */
esperanza::Result FinalizationState::IsVotable(const Validator &validator,
                                               const uint256 &targetHash,
                                               uint32_t targetEpoch,
                                               uint32_t sourceEpoch) const {
  auto validatorIndex = validator.m_validatorIndex;

  auto it = m_checkpoints.find(targetEpoch);
  if (it == m_checkpoints.end()) {
    return fail(esperanza::Result::VOTE_MALFORMED,
                "%s: the target epoch %d is in the future.\n", __func__,
                targetEpoch);
  }

  auto &targetCheckpoint = it->second;
  bool alreadyVoted = targetCheckpoint.m_voteMap.find(validatorIndex) !=
                      targetCheckpoint.m_voteMap.end();

  if (alreadyVoted) {
    return fail(esperanza::Result::VOTE_ALREADY_VOTED,
                "%s: the validator %s has already voted for target epoch %d.\n",
                __func__, validatorIndex.GetHex(), targetEpoch);
  }

  if (targetHash != m_recommendedTargetHash) {
    return fail(esperanza::Result::VOTE_WRONG_TARGET_HASH,
                "%s: the validator %s is voting for the %s, instead of the "
                "recommended targetHash %s.\n",
                __func__, validatorIndex.GetHex(), targetHash.GetHex(),
                m_recommendedTargetHash.GetHex());
  }

  if (targetEpoch != m_currentEpoch) {
    return fail(
        esperanza::Result::VOTE_WRONG_TARGET_EPOCH,
        "%s: the validator %s is voting for the wrong target epoch %d.\n",
        __func__, validatorIndex.GetHex(), targetEpoch);
  }

  it = m_checkpoints.find(sourceEpoch);
  if (it == m_checkpoints.end()) {
    return fail(esperanza::Result::VOTE_MALFORMED,
                "%s: the source epoch %d is in the future.\n", __func__,
                sourceEpoch);
  }

  auto &sourceCheckpoint = it->second;
  if (!sourceCheckpoint.m_isJustified) {
    return fail(
        esperanza::Result::VOTE_SRC_EPOCH_NOT_JUSTIFIED,
        "%s: the validator %s is voting for a non justified source epoch %d.\n",
        __func__, validatorIndex.GetHex(), targetEpoch);
  }

  if (IsInDynasty(validator, m_currentDynasty) ||
      IsInDynasty(validator, m_currentDynasty - 1)) {
    return success();
  }

  return fail(esperanza::Result::VOTE_NOT_VOTABLE,
              "%s: validator %s is not in dynasty %d nor the previous.\n",
              __func__, validatorIndex.GetHex(), m_currentDynasty);
}

/**
 * Validates the consistency of the deposit against the current state. This does
 * assume that the normal transaction validation process already took place.
 */
esperanza::Result FinalizationState::ValidateDeposit(
    const uint256 &validatorIndex, const CAmount &depositValue) const {
  LOCK(cs_esperanza);

  if (m_validators.find(validatorIndex) != m_validators.end()) {
    return fail(esperanza::Result::DEPOSIT_ALREADY_VALIDATOR,
                "%s: Validator with deposit hash of %s already "
                "exists.\n",
                __func__, validatorIndex.GetHex());
  }

  if (depositValue < MIN_DEPOSIT_SIZE) {
    return fail(esperanza::Result::DEPOSIT_INSUFFICIENT,
                "%s: The deposit value must be %d > %d.\n", __func__,
                depositValue, MIN_DEPOSIT_SIZE);
  }

  return success();
}

/**
 * Performs a deposit for the given amount and for the validator with the given
 * index.
 */
void FinalizationState::ProcessDeposit(const uint256 &validatorIndex,
                                       const CAmount &depositValue) {
  LOCK(cs_esperanza);

  uint32_t startDynasty = m_currentDynasty + 2;
  uint64_t scaledDeposit =
      ufp64::div_to_uint(static_cast<uint64_t>(depositValue),
                         m_depositScaleFactor[m_currentEpoch]);

  m_validators.insert(std::pair<uint256, Validator>(
      validatorIndex, Validator(scaledDeposit, startDynasty, validatorIndex)));

  m_dynastyDeltas[startDynasty] += scaledDeposit;

  LogPrint(BCLog::ESPERANZA,
           "%s: Add deposit %s for validator in dynasty %d.\n", __func__,
           validatorIndex.GetHex(), startDynasty);
}

uint64_t FinalizationState::CalculateVoteReward(
    const Validator &validator) const {
  return ufp64::mul_to_uint(m_rewardFactor, validator.m_deposit);
}

/**
 * Validates the consistency of the vote against the current state. This does
 * assume that the normal transaction validation process already took place.
 */
esperanza::Result FinalizationState::ValidateVote(const Vote &vote) const {
  LOCK(cs_esperanza);

  auto it = m_validators.find(vote.m_validatorIndex);
  if (it == m_validators.end()) {
    return fail(esperanza::Result::VOTE_NOT_BY_VALIDATOR,
                "%s: No validator with deposit hash of %s found.\n", __func__,
                vote.m_validatorIndex.GetHex());
  }

  esperanza::Result isVotable = IsVotable(
      it->second, vote.m_targetHash, vote.m_targetEpoch, vote.m_sourceEpoch);

  if (isVotable != +esperanza::Result::SUCCESS) {
    return fail(isVotable, "%s: The tuple (%s, %s, %d, %d) is not votable.\n",
                __func__, vote.m_validatorIndex.GetHex(),
                vote.m_targetHash.GetHex(), vote.m_sourceEpoch,
                vote.m_targetEpoch);
  }

  LogPrint(BCLog::ESPERANZA, "%s: Validator %s vote (%s, %d, %d) is valid.\n",
           __func__, vote.m_validatorIndex.GetHex(),
           vote.m_targetHash.GetHex(), vote.m_sourceEpoch, vote.m_targetEpoch);

  return success();
}

/**
 * Performs a vote using the given vote data.
 */
void FinalizationState::ProcessVote(const Vote &vote) {
  LOCK(cs_esperanza);

  m_checkpoints[vote.m_targetEpoch].m_voteMap.insert(vote.m_validatorIndex);

  LogPrint(BCLog::ESPERANZA,
           "%s: Validator %s voted successfully (%s, %d, %d).\n", __func__,
           vote.m_validatorIndex.GetHex(), vote.m_targetHash.GetHex(),
           vote.m_sourceEpoch, vote.m_targetEpoch);

  const uint256 &validatorIndex = vote.m_validatorIndex;
  uint32_t sourceEpoch = vote.m_sourceEpoch;
  uint32_t targetEpoch = vote.m_targetEpoch;
  const Validator &validator = m_validators[validatorIndex];

  bool inCurDynasty = IsInDynasty(validator, m_currentDynasty);
  bool inPrevDynasty = IsInDynasty(validator, m_currentDynasty - 1);

  uint64_t curDynastyVotes =
      m_checkpoints[targetEpoch].m_curDynastyVotes[sourceEpoch];

  uint64_t prevDynastyVotes =
      m_checkpoints[targetEpoch].m_prevDynastyVotes[sourceEpoch];

  if (inCurDynasty) {
    curDynastyVotes += validator.m_deposit;
    m_checkpoints[targetEpoch].m_curDynastyVotes[sourceEpoch] = curDynastyVotes;
  }

  if (inPrevDynasty) {
    prevDynastyVotes += validator.m_deposit;
    m_checkpoints[targetEpoch].m_prevDynastyVotes[sourceEpoch] =
        prevDynastyVotes;
  }

  if (m_expectedSrcEpoch == sourceEpoch) {
    uint64_t reward = CalculateVoteReward(validator);
    ProcessReward(validatorIndex, reward);
  }

  bool isTwoThirdsCurDyn =
      curDynastyVotes >=
      ufp64::div_to_uint(m_curDynDeposits * 2, ufp64::to_ufp64(3));

  bool isTwoThirdsPrevDyn =
      prevDynastyVotes >=
      ufp64::div_to_uint(m_prevDynDeposits * 2, ufp64::to_ufp64(3));

  bool enoughVotes = isTwoThirdsCurDyn && isTwoThirdsPrevDyn;

  if (enoughVotes && !m_checkpoints[targetEpoch].m_isJustified) {
    m_checkpoints[targetEpoch].m_isJustified = true;
    m_lastJustifiedEpoch = targetEpoch;
    m_mainHashJustified = true;

    LogPrint(BCLog::ESPERANZA, "%s: Epoch %d justified.\n", __func__,
             targetEpoch);

    if (targetEpoch == sourceEpoch + 1) {
      m_checkpoints[sourceEpoch].m_isFinalized = true;
      m_lastFinalizedEpoch = sourceEpoch;
      LogPrint(BCLog::ESPERANZA, "%s: Epoch %d finalized.\n", __func__,
               sourceEpoch);
    }
  }
  LogPrint(BCLog::ESPERANZA, "%s: Vote from validator %s processed.\n",
           __func__, validatorIndex.GetHex());
}

uint32_t FinalizationState::GetEndDynasty() const {
  return m_currentDynasty + DYNASTY_LOGOUT_DELAY;
}

/**
 * Validates the consistency of the logout against the current state. This does
 * assume that the normal transaction validation process already took place.
 * @param validatorIndex the index of the validator that is logging out
 * @return a representation of the outcome
 */
esperanza::Result FinalizationState::ValidateLogout(
    const uint256 &validatorIndex) const {
  LOCK(cs_esperanza);

  auto it = m_validators.find(validatorIndex);
  if (it == m_validators.end()) {
    return fail(esperanza::Result::LOGOUT_NOT_A_VALIDATOR,
                "%s: No validator with deposit hash of %s found.\n", __func__,
                validatorIndex.GetHex());
  }

  uint32_t endDynasty = GetEndDynasty();
  const Validator &validator = it->second;

  if (validator.m_startDynasty > m_currentDynasty) {
    return fail(esperanza::Result::LOGOUT_NOT_A_VALIDATOR,
                "%s: the validator with address %s is logging out before the "
                "start dynasty.\n",
                __func__, validator.m_validatorIndex.GetHex());
  }

  if (validator.m_endDynasty <= endDynasty) {
    return fail(esperanza::Result::LOGOUT_ALREADY_DONE,
                "%s: the validator with address %s already logget out.\n",
                __func__, validator.m_validatorIndex.GetHex());
  }

  return success();
}

/**
 * Performs a logout for the validator with the given index.
 */
void FinalizationState::ProcessLogout(const uint256 &validatorIndex) {
  LOCK(cs_esperanza);

  Validator &validator = m_validators[validatorIndex];

  uint32_t endDynasty = GetEndDynasty();
  validator.m_endDynasty = endDynasty;
  validator.m_depositsAtLogout = m_curDynDeposits;
  m_dynastyDeltas[endDynasty] -= validator.m_deposit;

  LogPrint(BCLog::ESPERANZA, "%s: Vote from validator %s logging out at %d.\n",
           __func__, validatorIndex.GetHex(), endDynasty);
}

/**
 * Validates a withdraw operation for the given validatorIndex.
 * @param validatorIndex
 * @return
 */
esperanza::Result FinalizationState::ValidateWithdraw(
    const uint256 &validatorIndex, const CAmount &requiredWithdraw) const {
  LOCK(cs_esperanza);

  CAmount withdrawAmountOut = 0;

  auto it = m_validators.find(validatorIndex);
  if (it == m_validators.end()) {
    return fail(esperanza::Result::WITHDRAW_NOT_A_VALIDATOR,
                "%s: No validator with deposit hash of %s found.\n", __func__,
                validatorIndex.GetHex());
  }

  const auto &validator = it->second;

  uint32_t endDynasty = validator.m_endDynasty;

  if (m_currentDynasty <= endDynasty) {
    return fail(esperanza::Result::WITHDRAW_BEFORE_END_DYNASTY,
                "%s: Too early to withdraw, minimum expected dynasty for "
                "withdraw is %d.\n",
                __func__, endDynasty);
  }

  uint32_t endEpoch = m_dynastyStartEpoch.find(endDynasty + 10)->second;
  uint32_t withdrawalEpoch = endEpoch + WITHDRAWAL_EPOCH_DELAY;

  if (m_currentEpoch <= withdrawalEpoch) {
    return fail(esperanza::Result::WITHDRAW_TOO_EARLY,
                "%s: Too early to withdraw, minimum expected epoch for "
                "withdraw is %d.\n",
                __func__, withdrawalEpoch);
  }

  if (!validator.m_isSlashed) {
    withdrawAmountOut = ufp64::mul_to_uint(
        m_depositScaleFactor.find(endEpoch)->second, validator.m_deposit);

  } else {
    uint32_t baseEpoch;
    if (2 * WITHDRAWAL_EPOCH_DELAY > withdrawalEpoch) {
      baseEpoch = 0;
    } else {
      baseEpoch = withdrawalEpoch - 2 * WITHDRAWAL_EPOCH_DELAY;
    }

    uint64_t recentlySlashed = m_totalSlashed.find(withdrawalEpoch)->second -
                               m_totalSlashed.find(baseEpoch)->second;
    ufp64::ufp64_t fractionToSlash =
        ufp64::div_2uint(recentlySlashed * SLASH_FRACTION_MULTIPLIER,
                         validator.m_depositsAtLogout);

    uint64_t depositSize =
        ufp64::mul_to_uint(m_depositScaleFactor.find(withdrawalEpoch)->second,
                           validator.m_deposit);

    if (fractionToSlash >= ufp64::to_ufp64(1)) {
      withdrawAmountOut = 0;
    } else {
      withdrawAmountOut = ufp64::mul_to_uint(
          ufp64::sub(ufp64::to_ufp64(1), fractionToSlash), depositSize);
    }

    LogPrint(BCLog::ESPERANZA, "%s: Withdraw from validator %s of %d units.\n",
             __func__, validatorIndex.GetHex(), endDynasty,
             withdrawAmountOut);
  }

  if (withdrawAmountOut != requiredWithdraw) {
    fail(esperanza::Result::WITHDRAW_WRONG_AMOUNT,
         "%s: Trying to withdraw %d, but only %d is valid.\n", __func__,
         requiredWithdraw, withdrawAmountOut);
  }

  return success();
}

/**
 * Performes a withdraw operation for the validator with the given index, in
 * fact removing him from the validators list.
 */
void FinalizationState::ProcessWithdraw(const uint256 &validatorIndex) {
  LOCK(cs_esperanza);

  DeleteValidator(validatorIndex);
}

/**
 * Checks whether two distinct votes from the same voter are proved being a
 * slashable misbehaviour.
 * @param vote1 the first vote.
 * @param vote2 the second vote.
 * @return true if the voter is slashable, false otherwise
 */
esperanza::Result FinalizationState::IsSlashable(const Vote &vote1,
                                                 const Vote &vote2) const {
  LOCK(cs_esperanza);

  auto it = m_validators.find(vote1.m_validatorIndex);
  if (it == m_validators.end()) {
    return fail(esperanza::Result::SLASH_NOT_A_VALIDATOR,
                "%s: No validator with deposit hash of %s found.\n", __func__,
                vote1.m_validatorIndex.GetHex());
  }
  const Validator &validator1 = it->second;

  it = m_validators.find(vote2.m_validatorIndex);
  if (it == m_validators.end()) {
    return fail(esperanza::Result::SLASH_NOT_A_VALIDATOR,
                "%s: No validator with deposit hash of %s found.\n", __func__,
                vote2.m_validatorIndex.GetHex());
  }
  const Validator &validator2 = it->second;

  uint256 validatorIndex1 = validator1.m_validatorIndex;
  uint256 validatorIndex2 = validator2.m_validatorIndex;

  uint32_t sourceEpoch1 = vote1.m_sourceEpoch;
  uint32_t targetEpoch1 = vote1.m_targetEpoch;

  uint32_t sourceEpoch2 = vote2.m_sourceEpoch;
  uint32_t targetEpoch2 = vote2.m_targetEpoch;

  if (validatorIndex1 != validatorIndex2) {
    return fail(esperanza::Result::SLASH_NOT_SAME_VALIDATOR,
                "%s: votes have not be casted by the same validator.\n",
                __func__);
  }

  if (validator1.m_startDynasty > m_currentDynasty) {
    return fail(esperanza::Result::SLASH_TOO_EARLY,
                "%s: validator with deposit hash %s is not yet voting.\n",
                __func__, vote1.m_validatorIndex.GetHex());
  }

  if (validator1.m_isSlashed) {
    return fail(
        esperanza::Result::SLASH_ALREADY_SLASHED,
        "%s: validator with deposit hash %s has been already slashed.\n",
        __func__, vote1.m_validatorIndex.GetHex());
  }

  if (vote1.m_targetHash == vote2.m_targetHash) {
    return fail(esperanza::Result::SLASH_SAME_VOTE,
                "%s: Not slashable cause the two votes are the same.\n",
                __func__);
  }

  bool isDoubleVote = targetEpoch1 == targetEpoch2;
  bool isSurroundVote =
      (targetEpoch1 > targetEpoch2 && sourceEpoch1 < sourceEpoch2) ||
      (targetEpoch2 > targetEpoch1 && sourceEpoch2 < sourceEpoch1);

  if (isDoubleVote || isSurroundVote) {
    return success();
  }

  return fail(esperanza::Result::SLASH_NOT_VALID, "%s: Slashing failed",
              __func__);
}

/**
 * Given two votes, performs a slash against the validator who performed them.
 * It also returns the bounty that the reporter shoul be awarded of.
 */
void FinalizationState::ProcessSlash(const Vote &vote1, const Vote &vote2,
                                     CAmount &slashingBountyOut) {
  LOCK(cs_esperanza);

  const uint256 &validatorIndex = vote1.m_validatorIndex;

  // Slash the offending validator, and give a 4% "finder's fee"
  CAmount validatorDeposit = GetDepositSize(validatorIndex);
  CAmount slashingBounty = validatorDeposit / BOUNTY_FRACTION_DENOMINATOR;
  m_totalSlashed[m_currentEpoch] += validatorDeposit;
  m_validators[validatorIndex].m_isSlashed = true;

  LogPrint(BCLog::ESPERANZA,
           "%s: Slashing validator with deposit hash %s of %d units, taking %d "
           "as bounty.\n",
           __func__, validatorIndex.GetHex(), validatorDeposit,
           slashingBounty);

  uint32_t endDynasty = m_validators[validatorIndex].m_endDynasty;

  // if validator not logged out yet, remove total from next dynasty
  // and forcibly logout next dynasty
  if (m_currentDynasty < endDynasty) {
    CAmount deposit = m_validators[validatorIndex].m_deposit;
    m_dynastyDeltas[m_currentDynasty + 1] -= deposit;
    m_validators[validatorIndex].m_endDynasty = m_currentDynasty + 1;

    // if validator was already staged for logout at end_dynasty,
    // ensure that we don't doubly remove from total
    if (endDynasty < DEFAULT_END_DYNASTY) {
      m_dynastyDeltas[endDynasty] += deposit;
    } else {
      // if no previously logged out, remember the total deposits at logout
      m_validators[validatorIndex].m_depositsAtLogout =
          GetTotalCurDynDeposits();
    }
  }

  slashingBountyOut = slashingBounty;
}

uint32_t FinalizationState::GetCurrentEpoch() const { return m_currentEpoch; }

uint32_t FinalizationState::GetLastJustifiedEpoch() const {
  return m_lastJustifiedEpoch;
}

uint32_t FinalizationState::GetLastFinalizedEpoch() const {
  return m_lastFinalizedEpoch;
}

uint32_t FinalizationState::GetCurrentDynasty() const {
  return m_currentDynasty;
}

/**
 * This method should return the right State instance that represents
 * the block before the given block. This method is gonna be used mostly
 * @param block
 * @return the state for the chain tip passed
 */
FinalizationState *FinalizationState::GetState(const CBlockIndex &blockIndex) {
  // UNIT-E: Replace the single instance with a map<block,state> to allow for
  // reorganizations.
  return esperanzaState.get();
}

/**
 * This method returns the State for the active chain tip.
 * @return the state for the active chain tip
 */
FinalizationState *FinalizationState::GetState() {
  // UNIT-E: Replace the single instance with a map<block,state> to allow for
  // reorganizations.
  return GetState(*chainActive.Tip());
}

uint32_t FinalizationState::GetEpoch(const CBlockIndex &blockIndex) {
  FinalizationState *state = GetState(blockIndex);

  return static_cast<uint32_t>(blockIndex.nHeight) / state->EPOCH_LENGTH;
}

std::vector<Validator> FinalizationState::GetValidators() const {
  std::vector<Validator> res;
  for (const auto &it : m_validators) {
    res.push_back(it.second);
  }
  return res;
}

bool FinalizationState::ValidateDepositAmount(CAmount amount) {
  return amount >= GetState()->MIN_DEPOSIT_SIZE;
}

void FinalizationState::Init(const esperanza::FinalizationParams &params) {
  LOCK(cs_init_lock);

  if (!esperanzaState) {
    esperanzaState = std::make_shared<FinalizationState>(params);
  }
}

/**
 * This method should encapsulate all the logic necessary to make the esperanza
 * state progress by one block.
 * @param blockIndex the index of the new block added.
 * @param block the new block added.
 * @return true if the method was successful, false otherwise.
 */
bool FinalizationState::ProcessNewTip(const CBlockIndex &blockIndex,
                                      const CBlock &block) {
  FinalizationState *state = GetState(blockIndex);

  LogPrint(BCLog::ESPERANZA, "%s: Processing block %d with hash %s.\n",
           __func__, blockIndex.nHeight, block.GetHash().GetHex());

  // We can skip everything for the genesis block since it isn't suppose to
  // contain esperanza's transactions.
  if (blockIndex.nHeight == 0) {
    return true;
  }

  // This is the first block of a new epoch.
  if (blockIndex.nHeight % state->EPOCH_LENGTH == 0) {
    state->InitializeEpoch(blockIndex.nHeight);
  }

  for (const auto &tx : block.vtx) {
    switch (tx->GetType()) {

      case TxType::DEPOSIT: {
        std::vector<std::vector<unsigned char>> vSolutions;
        txnouttype typeRet;

        if (Solver(tx->vout[0].scriptPubKey, typeRet, vSolutions)) {
          state->ProcessDeposit(CPubKey(vSolutions[0]).GetHash(),
                                tx->GetValueOut());
        }
        break;
      }

      case TxType::VOTE: {
        state->ProcessVote(CScript::ExtractVoteFromSignature(tx->vin[0].scriptSig));
        break;
      }

      default:
        assert(false);
    }
  }

  // This is the last block for the current epoch and it represent it, so we
  // update the targetHash.
  if (blockIndex.nHeight % state->EPOCH_LENGTH == state->EPOCH_LENGTH - 1) {
    LogPrint(
        BCLog::ESPERANZA,
        "%s: Last block of the epoch, the new recommended targetHash is %s.\n",
        __func__, block.GetHash().GetHex());
    state->m_recommendedTargetHash = block.GetHash();
  }

  return true;
}

}  // namespace esperanza
