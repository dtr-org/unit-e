// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/finalizationstate.h>

#include <chainparams.h>
#include <esperanza/checks.h>
#include <esperanza/vote.h>
#include <script/ismine.h>
#include <snapshot/creator.h>
#include <tinyformat.h>
#include <ufp64.h>
#include <util.h>
#include <validation.h>

#include <stdint.h>
#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace esperanza {

namespace {
//! \brief Storage of finalization states
//!
//! This cache keeps track of finalization states corresponding to block indexes.
class Storage {
 public:
  //! \brief Return finalization state for index, if any
  FinalizationState *Find(const CBlockIndex *index);

  //! \brief Try to find, then try to create new state for index.
  FinalizationState *FindOrCreate(const CBlockIndex *index);

  //! \brief Return state for genesis block
  FinalizationState *Genesis() const;

  //! \brief Destroy states for indexes with heights less than `height`
  void ClearUntilHeight(blockchain::Height height);

  //! \brief Reset the storage
  void Reset(const esperanza::FinalizationParams &params,
             const esperanza::AdminParams &admin_arams);

  //! \brief Reset the storage and initialize with fresh state for index (for prune mode)
  void ResetToTip(const esperanza::FinalizationParams &params,
                  const esperanza::AdminParams &admin_arams,
                  const CBlockIndex *index);

  //! \brief Restoring tells whether node is reconstructing finalization state
  bool Restoring() const {
    return m_restoring;
  }

  struct RestoringRAII {
    Storage &s;
    RestoringRAII(Storage &s) : s(s) { s.m_restoring = true; }
    ~RestoringRAII() { s.m_restoring = false; }
  };

 private:
  FinalizationState *Create(const CBlockIndex *index);

  mutable CCriticalSection cs;
  std::map<const CBlockIndex *, FinalizationState> m_states;
  std::unique_ptr<FinalizationState> m_genesis_state;
  std::atomic<bool> m_restoring;
};

Storage g_storage;
CCriticalSection cs_init_lock;
const ufp64::ufp64_t BASE_DEPOSIT_SCALE_FACTOR = ufp64::to_ufp64(1);
}  // namespace

template <typename... Args>
inline Result fail(Result error, const char *fmt, const Args &... args) {
  std::string reason = tfm::format(fmt, args...);
  LogPrint(BCLog::FINALIZATION, "ERROR: %s.\n", reason);
  return error;
}

inline Result success() { return Result::SUCCESS; }

FinalizationStateData::FinalizationStateData(const AdminParams &adminParams)
    : m_adminState(adminParams) {}

FinalizationState::FinalizationState(
    const esperanza::FinalizationParams &params,
    const esperanza::AdminParams &adminParams)
    : FinalizationStateData(adminParams), m_settings(params) {
  m_depositScaleFactor[0] = BASE_DEPOSIT_SCALE_FACTOR;
  m_totalSlashed[0] = 0;
  m_dynastyDeltas[0] = 0;

  Checkpoint cp = Checkpoint();
  cp.m_isJustified = true;
  cp.m_isFinalized = true;
  m_checkpoints[0] = cp;
}

FinalizationState::FinalizationState(const FinalizationState &parent)
    : FinalizationStateData(parent), m_settings(parent.m_settings) {}

Result FinalizationState::InitializeEpoch(blockchain::Height blockHeight) {
  LOCK(cs_esperanza);
  const auto newEpoch = GetEpoch(blockHeight);

  if (newEpoch != m_currentEpoch + 1) {
    return fail(Result::INIT_WRONG_EPOCH,
                "%s: Next epoch should be %d but %d was passed.\n", __func__,
                m_currentEpoch + 1, newEpoch);
  }

  LogPrint(BCLog::FINALIZATION, "%s: New epoch found, this epoch is the %d.\n",
           __func__, newEpoch);

  Checkpoint cp = Checkpoint();
  cp.m_curDynastyDeposits = GetTotalCurDynDeposits();
  cp.m_prevDynastyDeposits = GetTotalPrevDynDeposits();
  m_checkpoints[newEpoch] = cp;

  m_currentEpoch = newEpoch;

  LogPrint(BCLog::FINALIZATION, "%s: Epoch block found at height %d.\n",
           __func__, blockHeight);

  m_lastVoterRescale = ufp64::add_uint(GetCollectiveRewardFactor(), 1);

  m_lastNonVoterRescale = ufp64::div(m_lastVoterRescale, (ufp64::add_uint(m_rewardFactor, 1)));

  m_depositScaleFactor[newEpoch] = ufp64::mul(m_lastNonVoterRescale, GetDepositScaleFactor(newEpoch - 1));

  m_totalSlashed[newEpoch] = GetTotalSlashed(newEpoch - 1);

  if (DepositExists()) {
    ufp64::ufp64_t interestBase = ufp64::div(m_settings.base_interest_factor, GetSqrtOfTotalDeposits());

    m_rewardFactor = ufp64::add(interestBase, ufp64::mul_by_uint(m_settings.base_penalty_factor,
                                                                 GetEpochsSinceFinalization()));

    if (m_rewardFactor <= 0) {
      return fail(Result::INIT_INVALID_REWARD, "Invalid reward factor %d",
                  m_rewardFactor);
    }

  } else {
    InstaJustify();
    m_rewardFactor = 0;
  }

  IncrementDynasty();

  LogPrint(BCLog::FINALIZATION,
           "%s: Epoch=%d initialized. The current dynasty=%d.\n",
           __func__, newEpoch, m_currentDynasty);

  if (m_lastJustifiedEpoch < m_currentEpoch - 1) {
    LogPrint(BCLog::FINALIZATION,
             "%s: Epoch=%d was not justified. last_justified_epoch=%d last_finalized_epoch=%d.\n",
             __func__, m_currentEpoch - 1, m_lastJustifiedEpoch, m_lastFinalizedEpoch);
  }

  return success();
}

void FinalizationState::InstaJustify() {
  Checkpoint &cp = GetCheckpoint(m_currentEpoch - 1);
  cp.m_isJustified = true;
  m_lastJustifiedEpoch = m_currentEpoch - 1;

  if (m_currentEpoch > 1) {
    uint32_t expected_finalized = m_currentEpoch - 2;
    if (GetCheckpoint(expected_finalized).m_isJustified) {
      GetCheckpoint(expected_finalized).m_isFinalized = true;
      m_lastFinalizedEpoch = expected_finalized;
    }
  }

  TrimCache();

  LogPrint(BCLog::FINALIZATION, "%s: Justified epoch=%d.\n", __func__,
           m_lastJustifiedEpoch);
}

void FinalizationState::IncrementDynasty() {
  // finalized epoch is m_currentEpoch - 3 because:
  // finalized (0) - justified (1) - votes to justify (2) - current epoch (3)
  // TODO: UNIT-E: remove "m_currentEpoch > 2" when we delete instant finalization
  // and start epoch from 1 #570, #572
  if (m_currentEpoch > 2 && GetCheckpoint(m_currentEpoch - 3).m_isFinalized) {

    m_currentDynasty += 1;
    m_prevDynDeposits = m_curDynDeposits;
    m_curDynDeposits += GetDynastyDelta(m_currentDynasty);
    m_dynastyStartEpoch[m_currentDynasty] = m_currentEpoch;

    LogPrint(BCLog::FINALIZATION, "%s: New current dynasty=%d.\n", __func__,
             m_currentDynasty);
    // UNIT-E: we can clear old checkpoints (up to lastFinalizedEpoch - 1)
  }
  m_epochToDynasty[m_currentEpoch] = m_currentDynasty;
}

ufp64::ufp64_t FinalizationState::GetCollectiveRewardFactor() {
  uint32_t epoch = m_currentEpoch;
  bool isLive = GetEpochsSinceFinalization() <= 2;

  if (!DepositExists() || !isLive) {
    return 0;
  }

  ufp64::ufp64_t curVoteFraction = ufp64::div_2uint(
      GetCheckpoint(epoch - 1).GetCurDynastyVotes(m_expectedSourceEpoch),
      m_curDynDeposits);

  ufp64::ufp64_t prevVoteFraction = ufp64::div_2uint(
      GetCheckpoint(epoch - 1).GetPrevDynastyVotes(m_expectedSourceEpoch),
      m_prevDynDeposits);

  ufp64::ufp64_t voteFraction = ufp64::min(curVoteFraction, prevVoteFraction);

  return ufp64::div_by_uint(ufp64::mul(voteFraction, m_rewardFactor), 2);
}

bool FinalizationState::DepositExists() const {
  return m_curDynDeposits > 0 && m_prevDynDeposits > 0;
}

ufp64::ufp64_t FinalizationState::GetSqrtOfTotalDeposits() const {
  uint64_t totalDeposits = 1 + ufp64::mul_to_uint(GetDepositScaleFactor(m_currentEpoch - 1),
                                                  std::max(m_prevDynDeposits, m_curDynDeposits));

  return ufp64::sqrt_uint(totalDeposits);
}

uint32_t FinalizationState::GetEpochsSinceFinalization() const {
  return m_currentEpoch - m_lastFinalizedEpoch;
}

void FinalizationState::DeleteValidator(const uint160 &validatorAddress) {
  LOCK(cs_esperanza);

  m_validators.erase(validatorAddress);
}

uint64_t FinalizationState::GetDepositSize(const uint160 &validatorAddress) const {
  LOCK(cs_esperanza);

  auto validatorIt = m_validators.find(validatorAddress);
  auto depositScaleIt = m_depositScaleFactor.find(m_currentEpoch);

  if (validatorIt != m_validators.end() &&
      !validatorIt->second.m_isSlashed &&
      depositScaleIt != m_depositScaleFactor.end()) {

    return ufp64::mul_to_uint(depositScaleIt->second, validatorIt->second.m_deposit);
  } else {
    return 0;
  }
}

const CBlockIndex *FinalizationState::GetRecommendedTarget() const {
  return m_recommendedTarget;
}

Vote FinalizationState::GetRecommendedVote(const uint160 &validatorAddress) const {
  LOCK(cs_esperanza);

  assert(m_recommendedTarget && "m_recommendedTarget must be set!");

  Vote vote;
  vote.m_validatorAddress = validatorAddress;
  vote.m_targetHash = m_recommendedTarget->GetBlockHash();
  vote.m_targetEpoch = GetEpoch(*m_recommendedTarget);
  vote.m_sourceEpoch = m_expectedSourceEpoch;

  LogPrint(BCLog::FINALIZATION,
           "%s: source_epoch=%d target_epoch=%d dynasty=%d target_hash=%s.\n",
           __func__,
           vote.m_sourceEpoch,
           vote.m_targetEpoch,
           m_currentDynasty,
           vote.m_targetHash.GetHex());

  return vote;
}

bool FinalizationState::IsInDynasty(const Validator &validator, uint32_t dynasty) const {

  uint32_t startDynasty = validator.m_startDynasty;
  uint32_t endDynasty = validator.m_endDynasty;
  return (startDynasty <= dynasty) && (dynasty < endDynasty);
}

uint64_t FinalizationState::GetTotalCurDynDeposits() const {

  return ufp64::mul_to_uint(GetDepositScaleFactor(m_currentEpoch),
                            m_curDynDeposits);
}

uint64_t FinalizationState::GetTotalPrevDynDeposits() const {

  if (m_currentEpoch == 0) {
    return 0;
  }

  return ufp64::mul_to_uint(GetDepositScaleFactor(m_currentEpoch - 1), m_prevDynDeposits);
}

CAmount FinalizationState::ProcessReward(const uint160 &validatorAddress, uint64_t reward) {

  Validator &validator = m_validators.at(validatorAddress);
  validator.m_deposit = validator.m_deposit + reward;
  uint32_t startDynasty = validator.m_startDynasty;
  uint32_t endDynasty = validator.m_endDynasty;

  if ((startDynasty <= m_currentDynasty) && (m_currentDynasty < endDynasty)) {
    m_curDynDeposits += reward;
  }

  if ((startDynasty <= m_currentDynasty - 1) && (m_currentDynasty - 1 < endDynasty)) {
    m_prevDynDeposits += reward;
  }

  if (endDynasty < DEFAULT_END_DYNASTY) {
    m_dynastyDeltas[endDynasty] = GetDynastyDelta(endDynasty) - reward;
  }

  return ufp64::mul_to_uint(GetDepositScaleFactor(m_currentEpoch),
                            validator.m_deposit);

  // UNIT-E: Here is where we should reward proposers if we want
}

Result FinalizationState::IsVotable(const Validator &validator,
                                    const uint256 &targetHash,
                                    uint32_t targetEpoch,
                                    uint32_t sourceEpoch) const {

  auto validatorAddress = validator.m_validatorAddress;

  auto it = m_checkpoints.find(targetEpoch);
  if (it == m_checkpoints.end()) {
    return fail(Result::VOTE_MALFORMED,
                "%s: target_epoch=%d is in the future.\n", __func__,
                targetEpoch);
  }

  auto &targetCheckpoint = it->second;
  bool alreadyVoted = targetCheckpoint.m_voteSet.find(validatorAddress) !=
                      targetCheckpoint.m_voteSet.end();

  if (alreadyVoted) {
    return fail(Result::VOTE_ALREADY_VOTED,
                "%s: validator=%s has already voted for target_epoch=%d.\n",
                __func__, validatorAddress.GetHex(), targetEpoch);
  }

  if (targetHash != m_recommendedTarget->GetBlockHash()) {
    return fail(Result::VOTE_WRONG_TARGET_HASH,
                "%s: validator=%s is voting for target=%s instead of the "
                "recommended_target=%s.\n",
                __func__, validatorAddress.GetHex(), targetHash.GetHex(),
                m_recommendedTarget->GetBlockHash().GetHex());
  }

  if (targetEpoch != m_currentEpoch - 1) {
    return fail(
        Result::VOTE_WRONG_TARGET_EPOCH,
        "%s: vote for wrong target_epoch=%d. validator=%s current_epoch=%d\n",
        __func__, targetEpoch, validatorAddress.GetHex(), m_currentEpoch);
  }

  it = m_checkpoints.find(sourceEpoch);
  if (it == m_checkpoints.end()) {
    return fail(Result::VOTE_MALFORMED,
                "%s: source_epoch=%d is in the future. current_epoch=%d\n", __func__,
                sourceEpoch, m_currentEpoch);
  }

  auto &sourceCheckpoint = it->second;
  if (!sourceCheckpoint.m_isJustified) {
    return fail(
        Result::VOTE_SRC_EPOCH_NOT_JUSTIFIED,
        "%s: validator=%s is voting for a non justified source epoch=%d.\n",
        __func__, validatorAddress.GetHex(), targetEpoch);
  }

  if (IsInDynasty(validator, m_currentDynasty) || IsInDynasty(validator, m_currentDynasty - 1)) {
    return success();
  }

  return fail(Result::VOTE_NOT_VOTABLE,
              "%s: validator=%s is not in dynasty=%d nor the previous.\n",
              __func__, validatorAddress.GetHex(), m_currentDynasty);
}

Result FinalizationState::ValidateDeposit(const uint160 &validatorAddress,
                                          CAmount depositValue) const {
  LOCK(cs_esperanza);

  if (!m_adminState.IsValidatorAuthorized(validatorAddress)) {
    return fail(esperanza::Result::ADMIN_BLACKLISTED,
                "%s: validator=%s is blacklisted.\n", __func__,
                validatorAddress.GetHex());
  }

  if (m_validators.find(validatorAddress) != m_validators.end()) {
    return fail(Result::DEPOSIT_ALREADY_VALIDATOR,
                "%s: validator=%s with the deposit already exists.\n",
                __func__, validatorAddress.GetHex());
  }

  if (depositValue < m_settings.min_deposit_size) {
    return fail(Result::DEPOSIT_INSUFFICIENT,
                "%s: The deposit value must be %d > %d.\n", __func__,
                depositValue, m_settings.min_deposit_size);
  }

  return success();
}

void FinalizationState::ProcessDeposit(const uint160 &validatorAddress,
                                       CAmount depositValue) {
  LOCK(cs_esperanza);

  uint32_t startDynasty = m_currentDynasty + 2;
  uint64_t scaledDeposit = ufp64::div_to_uint(static_cast<uint64_t>(depositValue),
                                              GetDepositScaleFactor(m_currentEpoch));

  m_validators.insert(std::pair<uint160, Validator>(
      validatorAddress,
      Validator(scaledDeposit, startDynasty, validatorAddress)));

  m_dynastyDeltas[startDynasty] = GetDynastyDelta(startDynasty) + scaledDeposit;

  LogPrint(BCLog::FINALIZATION,
           "%s: Add deposit %s for validator in dynasty %d.\n", __func__,
           validatorAddress.GetHex(), startDynasty);
}

uint64_t FinalizationState::CalculateVoteReward(const Validator &validator) const {
  return ufp64::mul_to_uint(m_rewardFactor, validator.m_deposit);
}

Result FinalizationState::ValidateVote(const Vote &vote) const {
  LOCK(cs_esperanza);

  if (!m_adminState.IsValidatorAuthorized(vote.m_validatorAddress)) {
    return fail(esperanza::Result::ADMIN_BLACKLISTED,
                "%s: validator=%s is blacklisted\n", __func__,
                vote.m_validatorAddress.GetHex());
  }

  auto it = m_validators.find(vote.m_validatorAddress);
  if (it == m_validators.end()) {
    return fail(Result::VOTE_NOT_BY_VALIDATOR,
                "%s: No validator with index %s found.\n", __func__,
                vote.m_validatorAddress.GetHex());
  }

  Result isVotable = IsVotable(it->second, vote.m_targetHash,
                               vote.m_targetEpoch, vote.m_sourceEpoch);

  if (isVotable != +Result::SUCCESS) {
    return fail(isVotable, "%s: not votable. validator=%s target=%s source_epoch=%d target_epoch=%d\n",
                __func__,
                vote.m_validatorAddress.GetHex(),
                vote.m_targetHash.GetHex(),
                vote.m_sourceEpoch,
                vote.m_targetEpoch);
  }

  LogPrint(BCLog::FINALIZATION,
           "%s: valid. validator=%s target=%s source_epoch=%d target_epoch=%d\n",
           __func__,
           vote.m_validatorAddress.GetHex(),
           vote.m_targetHash.GetHex(),
           vote.m_sourceEpoch,
           vote.m_targetEpoch);

  return success();
}

void FinalizationState::ProcessVote(const Vote &vote) {
  LOCK(cs_esperanza);

  GetCheckpoint(vote.m_targetEpoch).m_voteSet.insert(vote.m_validatorAddress);

  LogPrint(BCLog::FINALIZATION,
           "%s: validator=%s voted successfully. target=%s source_epoch=%d target_epoch=%d.\n",
           __func__,
           vote.m_validatorAddress.GetHex(),
           vote.m_targetHash.GetHex(),
           vote.m_sourceEpoch,
           vote.m_targetEpoch);

  const uint160 &validatorAddress = vote.m_validatorAddress;
  uint32_t sourceEpoch = vote.m_sourceEpoch;
  uint32_t targetEpoch = vote.m_targetEpoch;
  const Validator &validator = m_validators.at(validatorAddress);

  bool inCurDynasty = IsInDynasty(validator, m_currentDynasty);
  bool inPrevDynasty = IsInDynasty(validator, m_currentDynasty - 1);

  uint64_t curDynastyVotes = GetCheckpoint(targetEpoch).GetCurDynastyVotes(sourceEpoch);

  uint64_t prevDynastyVotes = GetCheckpoint(targetEpoch).GetPrevDynastyVotes(sourceEpoch);

  if (inCurDynasty) {
    curDynastyVotes += validator.m_deposit;
    GetCheckpoint(targetEpoch).m_curDynastyVotes[sourceEpoch] = curDynastyVotes;
  }

  if (inPrevDynasty) {
    prevDynastyVotes += validator.m_deposit;
    GetCheckpoint(targetEpoch).m_prevDynastyVotes[sourceEpoch] = prevDynastyVotes;
  }

  if (m_expectedSourceEpoch == sourceEpoch) {
    uint64_t reward = CalculateVoteReward(validator);
    ProcessReward(validatorAddress, reward);
  }

  bool isTwoThirdsCurDyn =
      curDynastyVotes >= ufp64::div_to_uint(m_curDynDeposits * 2, ufp64::to_ufp64(3));

  bool isTwoThirdsPrevDyn =
      prevDynastyVotes >= ufp64::div_to_uint(m_prevDynDeposits * 2, ufp64::to_ufp64(3));

  bool enoughVotes = isTwoThirdsCurDyn && isTwoThirdsPrevDyn;

  if (enoughVotes && !GetCheckpoint(targetEpoch).m_isJustified) {

    GetCheckpoint(targetEpoch).m_isJustified = true;
    m_lastJustifiedEpoch = targetEpoch;

    LogPrint(BCLog::FINALIZATION, "%s: epoch=%d justified.\n", __func__,
             targetEpoch);

    if (targetEpoch == sourceEpoch + 1) {
      GetCheckpoint(sourceEpoch).m_isFinalized = true;
      m_lastFinalizedEpoch = sourceEpoch;
      TrimCache();
      LogPrint(BCLog::FINALIZATION, "%s: epoch=%d finalized.\n", __func__,
               sourceEpoch);
    }
  }
  LogPrint(BCLog::FINALIZATION, "%s: vote from validator=%s processed.\n",
           __func__, validatorAddress.GetHex());
}

uint32_t FinalizationState::GetEndDynasty() const {
  return m_currentDynasty + m_settings.dynasty_logout_delay;
}

Result FinalizationState::ValidateLogout(const uint160 &validatorAddress) const {
  LOCK(cs_esperanza);

  auto it = m_validators.find(validatorAddress);
  if (it == m_validators.end()) {
    return fail(Result::LOGOUT_NOT_A_VALIDATOR,
                "%s: No validator with index %s found.\n", __func__,
                validatorAddress.GetHex());
  }

  uint32_t endDynasty = GetEndDynasty();
  const Validator &validator = it->second;

  if (validator.m_startDynasty > m_currentDynasty) {
    return fail(Result::LOGOUT_NOT_A_VALIDATOR,
                "%s: the validator with address %s is logging out before the "
                "start dynasty.\n",
                __func__, validator.m_validatorAddress.GetHex());
  }

  if (validator.m_endDynasty <= endDynasty) {
    return fail(Result::LOGOUT_ALREADY_DONE,
                "%s: validator=%s already logged out.\n",
                __func__, validator.m_validatorAddress.GetHex());
  }

  return success();
}

void FinalizationState::ProcessLogout(const uint160 &validatorAddress) {
  LOCK(cs_esperanza);

  Validator &validator = m_validators.at(validatorAddress);

  uint32_t endDyn = GetEndDynasty();
  validator.m_endDynasty = endDyn;
  validator.m_depositsAtLogout = m_curDynDeposits;
  m_dynastyDeltas[endDyn] = GetDynastyDelta(endDyn) - validator.m_deposit;

  LogPrint(BCLog::FINALIZATION,
           "%s: validator=%s logging out at dynasty=%d.\n", __func__,
           validatorAddress.GetHex(), endDyn);
}

Result FinalizationState::ValidateWithdraw(const uint160 &validatorAddress,
                                           CAmount requestedWithdraw) const {
  LOCK(cs_esperanza);

  CAmount withdrawableAmount = 0;

  Result res = CalculateWithdrawAmount(validatorAddress, withdrawableAmount);

  if (res != +Result::SUCCESS) {
    return res;
  }

  if (withdrawableAmount < requestedWithdraw) {
    fail(Result::WITHDRAW_WRONG_AMOUNT,
         "%s: Trying to withdraw %d, but max is %d.\n", __func__,
         requestedWithdraw, withdrawableAmount);
  }

  return success();
}

Result FinalizationState::CalculateWithdrawAmount(const uint160 &validatorAddress,
                                                  CAmount &withdrawAmountOut) const {
  LOCK(cs_esperanza);

  withdrawAmountOut = 0;

  auto it = m_validators.find(validatorAddress);
  if (it == m_validators.end()) {
    return fail(Result::WITHDRAW_NOT_A_VALIDATOR,
                "%s: No validator with index %s found.\n", __func__,
                validatorAddress.GetHex());
  }

  const auto &validator = it->second;

  uint32_t endDynasty = validator.m_endDynasty;

  if (m_currentDynasty <= endDynasty) {
    return fail(Result::WITHDRAW_BEFORE_END_DYNASTY,
                "%s: Too early to withdraw, minimum expected dynasty for "
                "withdraw is %d.\n",
                __func__, endDynasty);
  }

  uint32_t endEpoch = m_dynastyStartEpoch.find(endDynasty + 1)->second;
  uint32_t withdrawalEpoch = endEpoch + m_settings.withdrawal_epoch_delay;

  if (m_currentEpoch < withdrawalEpoch) {
    return fail(Result::WITHDRAW_TOO_EARLY,
                "%s: Too early to withdraw, minimum expected epoch for "
                "withdraw is %d.\n",
                __func__, withdrawalEpoch);
  }

  if (!validator.m_isSlashed) {
    withdrawAmountOut = ufp64::mul_to_uint(GetDepositScaleFactor(endEpoch),
                                           validator.m_deposit);

  } else {
    uint32_t baseEpoch;
    if (2 * m_settings.withdrawal_epoch_delay > withdrawalEpoch) {
      baseEpoch = 0;
    } else {
      baseEpoch = withdrawalEpoch - 2 * m_settings.withdrawal_epoch_delay;
    }

    uint64_t recentlySlashed = GetTotalSlashed(withdrawalEpoch) - GetTotalSlashed(baseEpoch);

    ufp64::ufp64_t fractionToSlash = ufp64::div_2uint(recentlySlashed * m_settings.slash_fraction_multiplier,
                                                      validator.m_depositsAtLogout);

    uint64_t depositSize = ufp64::mul_to_uint(GetDepositScaleFactor(withdrawalEpoch), validator.m_deposit);

    if (fractionToSlash >= ufp64::to_ufp64(1)) {
      withdrawAmountOut = 0;
    } else {
      withdrawAmountOut = ufp64::mul_to_uint(ufp64::sub(ufp64::to_ufp64(1), fractionToSlash), depositSize);
    }

    LogPrint(BCLog::FINALIZATION,
             "%s: Withdraw from validator %s of %d units.\n", __func__,
             validatorAddress.GetHex(), endDynasty, withdrawAmountOut);
  }

  return success();
}

void FinalizationState::ProcessWithdraw(const uint160 &validatorAddress) {
  LOCK(cs_esperanza);

  DeleteValidator(validatorAddress);
}

bool FinalizationState::IsPermissioningActive() const {
  return m_adminState.IsPermissioningActive();
}

void FinalizationState::OnBlock(blockchain::Height blockHeight) {
  m_adminState.OnBlock(blockHeight);
}

Result FinalizationState::ValidateAdminKeys(const AdminKeySet &adminKeys) const {
  LOCK(cs_esperanza);

  if (m_adminState.IsAdminAuthorized(adminKeys)) {
    return esperanza::Result::SUCCESS;
  }

  return fail(esperanza::Result::ADMIN_NOT_AUTHORIZED,
              "Provided pubkeys do not belong to admin");
}

void FinalizationState::ProcessAdminCommands(
    const std::vector<AdminCommand> &commands) {
  LOCK(cs_esperanza);

  for (const auto &command : commands) {
    switch (command.GetCommandType()) {
      case AdminCommandType::ADD_TO_WHITELIST: {
        for (const auto &pubkey : command.GetPayload()) {
          m_adminState.AddValidator(pubkey.GetID());
        }
        break;
      }
      case AdminCommandType::REMOVE_FROM_WHITELIST: {
        for (const auto &pubkey : command.GetPayload()) {
          m_adminState.RemoveValidator(pubkey.GetID());
        }
        break;
      }
      case AdminCommandType::RESET_ADMINS: {
        const auto &pubkeys = command.GetPayload();
        AdminKeySet set;
        std::copy_n(pubkeys.begin(), ADMIN_MULTISIG_KEYS, set.begin());
        m_adminState.ResetAdmin(set);
        break;
      }
      case AdminCommandType::END_PERMISSIONING: {
        m_adminState.EndPermissioning();
        break;
      }
    }
  }
}

Result FinalizationState::IsSlashable(const Vote &vote1,
                                      const Vote &vote2) const {
  LOCK(cs_esperanza);

  auto it = m_validators.find(vote1.m_validatorAddress);
  if (it == m_validators.end()) {
    return fail(Result::SLASH_NOT_A_VALIDATOR,
                "%s: No validator with index %s found.\n", __func__,
                vote1.m_validatorAddress.GetHex());
  }
  const Validator &validator1 = it->second;

  it = m_validators.find(vote2.m_validatorAddress);
  if (it == m_validators.end()) {
    return fail(Result::SLASH_NOT_A_VALIDATOR,
                "%s: No validator with index %s found.\n", __func__,
                vote2.m_validatorAddress.GetHex());
  }
  const Validator &validator2 = it->second;

  uint160 validatorAddress1 = validator1.m_validatorAddress;
  uint160 validatorAddress2 = validator2.m_validatorAddress;

  uint32_t sourceEpoch1 = vote1.m_sourceEpoch;
  uint32_t targetEpoch1 = vote1.m_targetEpoch;

  uint32_t sourceEpoch2 = vote2.m_sourceEpoch;
  uint32_t targetEpoch2 = vote2.m_targetEpoch;

  if (validatorAddress1 != validatorAddress2) {
    return fail(Result::SLASH_NOT_SAME_VALIDATOR,
                "%s: votes have not be casted by the same validator.\n",
                __func__);
  }

  if (validator1.m_startDynasty > m_currentDynasty) {
    return fail(Result::SLASH_TOO_EARLY,
                "%s: validator with deposit hash %s is not yet voting.\n",
                __func__, vote1.m_validatorAddress.GetHex());
  }

  if (validator1.m_isSlashed) {
    return fail(
        Result::SLASH_ALREADY_SLASHED,
        "%s: validator with deposit hash %s has been already slashed.\n",
        __func__, vote1.m_validatorAddress.GetHex());
  }

  if (vote1.m_targetHash == vote2.m_targetHash) {
    return fail(Result::SLASH_SAME_VOTE,
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

  return fail(Result::SLASH_NOT_VALID, "%s: Slashing failed", __func__);
}

void FinalizationState::ProcessSlash(const Vote &vote1, const Vote &vote2) {
  LOCK(cs_esperanza);

  const uint160 &validatorAddress = vote1.m_validatorAddress;

  const CAmount validatorDeposit = GetDepositSize(validatorAddress);

  m_totalSlashed[m_currentEpoch] =
      GetTotalSlashed(m_currentEpoch) + validatorDeposit;

  m_validators.at(validatorAddress).m_isSlashed = true;

  LogPrint(BCLog::FINALIZATION,
           "%s: Slashing validator with deposit hash %s of %d units.\n",
           __func__, validatorAddress.GetHex(), validatorDeposit);

  const uint32_t endDynasty = m_validators.at(validatorAddress).m_endDynasty;

  // if validator not logged out yet, remove total from next dynasty
  // and forcibly logout next dynasty
  if (m_currentDynasty < endDynasty) {
    const CAmount deposit = m_validators.at(validatorAddress).m_deposit;
    m_dynastyDeltas[m_currentDynasty + 1] =
        GetDynastyDelta(m_currentDynasty + 1) - deposit;
    m_validators.at(validatorAddress).m_endDynasty = m_currentDynasty + 1;

    // if validator was already staged for logout at end_dynasty,
    // ensure that we don't doubly remove from total
    if (endDynasty < DEFAULT_END_DYNASTY) {
      m_dynastyDeltas[endDynasty] = GetDynastyDelta(endDynasty) + deposit;
    } else {
      // if no previously logged out, remember the total deposits at logout
      m_validators.at(validatorAddress).m_depositsAtLogout =
          GetTotalCurDynDeposits();
    }
  }
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

FinalizationState *FinalizationState::GetState(const CBlockIndex *block_index) {
  if (block_index == nullptr) {
    block_index = chainActive.Tip();
  }
  return g_storage.Find(block_index);
}

uint32_t FinalizationState::GetEpochLength() const {
  return m_settings.epoch_length;
}

uint32_t FinalizationState::GetEpoch(const CBlockIndex &blockIndex) const {
  return GetEpoch(blockIndex.nHeight);
}

uint32_t FinalizationState::GetEpoch(blockchain::Height blockHeight) const {
  return blockHeight / GetEpochLength();
}

std::vector<Validator> FinalizationState::GetValidators() const {
  std::vector<Validator> res;
  for (const auto &it : m_validators) {
    res.push_back(it.second);
  }
  return res;
}

const Validator *FinalizationState::GetValidator(const uint160 &validatorAddress) const {

  auto it = m_validators.find(validatorAddress);

  if (it != m_validators.end()) {
    return &it->second;
  } else {
    return nullptr;
  }
}

bool FinalizationState::ValidateDepositAmount(CAmount amount) {
  return amount >= GetState()->m_settings.min_deposit_size;
}

void FinalizationState::Init(const esperanza::FinalizationParams &params,
                             const esperanza::AdminParams &admin_params) {
  LOCK(cs_init_lock);
  g_storage.Reset(params, admin_params);
}

void FinalizationState::Reset(const esperanza::FinalizationParams &params,
                              const esperanza::AdminParams &admin_params) {
  LogPrint(BCLog::FINALIZATION, "Completely reset finalization state\n");
  LOCK(cs_init_lock);
  g_storage.Reset(params, admin_params);
}

void FinalizationState::ResetToTip(const CBlockIndex &index) {
  LogPrint(BCLog::FINALIZATION, "Restore finalization state to tip: %s\n",
           index.GetBlockHash().GetHex());
  LOCK(cs_init_lock);
  const auto state = g_storage.Genesis();
  assert(state != nullptr);
  g_storage.ResetToTip(state->m_settings, state->m_adminState.GetParams(), &index);
}

void FinalizationState::ProcessNewCommit(const CTransactionRef &tx) {
  switch (tx->GetType()) {
    case TxType::VOTE: {
      Vote vote;
      std::vector<unsigned char> voteSig;
      assert(CScript::ExtractVoteFromVoteSignature(tx->vin[0].scriptSig, vote, voteSig));
      ProcessVote(vote);
      RegisterLastTx(vote.m_validatorAddress, tx);
      break;
    }

    case TxType::DEPOSIT: {
      uint160 validatorAddress = uint160();

      assert(ExtractValidatorAddress(*tx, validatorAddress));
      ProcessDeposit(validatorAddress, tx->vout[0].nValue);
      RegisterLastTx(validatorAddress, tx);
      break;
    }

    case TxType::LOGOUT: {
      uint160 validatorAddress = uint160();

      assert(ExtractValidatorAddress(*tx, validatorAddress));
      ProcessLogout(validatorAddress);
      RegisterLastTx(validatorAddress, tx);
      break;
    }

    case TxType::WITHDRAW: {
      uint160 validatorAddress = uint160();

      assert(ExtractValidatorAddress(*tx, validatorAddress));
      ProcessWithdraw(validatorAddress);
      break;
    }

    case TxType::SLASH: {

      esperanza::Vote vote1;
      esperanza::Vote vote2;
      std::vector<unsigned char> voteSig1;
      std::vector<unsigned char> voteSig2;
      CScript::ExtractVotesFromSlashSignature(tx->vin[0].scriptSig, vote1,
                                              vote2, voteSig1, voteSig2);

      ProcessSlash(vote1, vote2);
      break;
    }

    case TxType::ADMIN: {
      std::vector<AdminCommand> commands;
      for (const auto &output : tx->vout) {
        AdminCommand command;
        if (!MatchAdminCommand(output.scriptPubKey)) {
          continue;
        }
        assert(DecodeAdminCommand(output.scriptPubKey, command));
        commands.emplace_back(std::move(command));
      }

      ProcessAdminCommands(commands);
      break;
    }

    case TxType::COINBASE:
    case TxType::STANDARD:
      break;
  }
}

bool FinalizationState::ProcessNewTip(const CBlockIndex &block_index,
                                      const CBlock &block) {
  return ProcessNewCommits(block_index, block.vtx);
}

bool FinalizationState::ProcessNewCommits(const CBlockIndex &block_index,
                                          const std::vector<CTransactionRef> &txes) {
  uint256 block_hash = block_index.GetBlockHash();

  // Used to apply hardcoded parameters for a given block
  OnBlock(block_index.nHeight);

  // We can skip everything for the genesis block since it isn't suppose to
  // contain esperanza's transactions.
  if (block_index.nHeight == 0) {
    return true;
  }

  // This is the first block of a new epoch.
  if (block_index.nHeight % m_settings.epoch_length == 0) {
    InitializeEpoch(block_index.nHeight);
  }

  for (const auto &tx : txes) {
    ProcessNewCommit(tx);
  }

  if (!g_storage.Restoring() && (block_index.nHeight + 2) % m_settings.epoch_length == 0) {
    // Generate the snapshot for the block which is one block behind the last one.
    // The last epoch block will contain the snapshot hash pointing to this snapshot.
    snapshot::Creator::GenerateOrSkip(m_currentEpoch);
  }

  // This is the last block for the current epoch and it represent it, so we
  // update the targetHash.
  if (block_index.nHeight % m_settings.epoch_length == m_settings.epoch_length - 1) {
    LogPrint(
        BCLog::FINALIZATION,
        "%s: Last block of the epoch, the new recommended targetHash is %s.\n",
        __func__, block_hash.GetHex());

    m_recommendedTarget = &block_index;
    m_expectedSourceEpoch = m_lastJustifiedEpoch;

    // mark snapshots finalized up to the last finalized block
    blockchain::Height height = (m_lastFinalizedEpoch + 1) * m_settings.epoch_length - 1;
    if (height == static_cast<blockchain::Height>(block_index.nHeight)) {  // instant confirmation
      snapshot::Creator::FinalizeSnapshots(&block_index);
    } else {
      snapshot::Creator::FinalizeSnapshots(chainActive[height]);
    }
  }
  return true;
}

// Private accessors used to avoid map's operator[] potential side effects.
ufp64::ufp64_t FinalizationState::GetDepositScaleFactor(uint32_t epoch) const {
  const auto it = m_depositScaleFactor.find(epoch);
  assert(it != m_depositScaleFactor.end());
  return it->second;
}

uint64_t FinalizationState::GetTotalSlashed(uint32_t epoch) const {
  const auto it = m_totalSlashed.find(epoch);
  assert(it != m_totalSlashed.end());
  return it->second;
}

uint64_t FinalizationState::GetDynastyDelta(uint32_t dynasty) {
  const auto pair = m_dynastyDeltas.emplace(dynasty, 0);
  return pair.first->second;
}

Checkpoint &FinalizationState::GetCheckpoint(uint32_t epoch) {
  const auto it = m_checkpoints.find(epoch);
  assert(it != m_checkpoints.end());
  return it->second;
}

void FinalizationState::RegisterLastTx(uint160 &validatorAddress,
                                       CTransactionRef tx) {

  Validator &validator = m_validators.at(validatorAddress);
  validator.m_lastTransactionHash = tx->GetHash();
}

uint256 FinalizationState::GetLastTxHash(uint160 &validatorAddress) const {
  const Validator &validator = m_validators.at(validatorAddress);
  return validator.m_lastTransactionHash;
}

bool FinalizationState::IsCheckpoint(blockchain::Height blockHeight) const {
  return (blockHeight + 1) % m_settings.epoch_length == 0;
}

bool FinalizationState::IsJustifiedCheckpoint(blockchain::Height blockHeight) const {
  if (!IsCheckpoint(blockHeight)) {
    return false;
  }
  auto const it = m_checkpoints.find(GetEpoch(blockHeight));
  return it != m_checkpoints.end() && it->second.m_isJustified;
}

bool FinalizationState::IsFinalizedCheckpoint(blockchain::Height blockHeight) const {
  if (!IsCheckpoint(blockHeight)) {
    return false;
  }
  auto const it = m_checkpoints.find(GetEpoch(blockHeight));
  return it != m_checkpoints.end() && it->second.m_isFinalized;
}

void FinalizationState::TrimCache() {
  // last finalized checkpoint
  const auto height = (GetLastFinalizedEpoch() + 1) * GetEpochLength() - 1;
  LogPrint(BCLog::FINALIZATION, "Removing finalization states for height < %d\n", height);
  g_storage.ClearUntilHeight(height);
}

// Storage implementation section

FinalizationState *Storage::Find(const CBlockIndex *index) {
  LOCK(cs);
  if (index == nullptr) {
    return nullptr;
  }
  if (index->nHeight == 0) {
    return Genesis();
  }
  const auto it = m_states.find(index);
  if (it == m_states.end()) {
    return nullptr;
  } else {
    return &it->second;
  }
}

FinalizationState *Storage::Create(const CBlockIndex *index) {
  AssertLockHeld(cs);
  if (index->pprev == nullptr) {
    return nullptr;
  }
  const auto parent = Find(index->pprev);
  if (parent == nullptr) {
    return nullptr;
  }
  const auto res = m_states.emplace(index, FinalizationState(*parent));
  return &res.first->second;
}

FinalizationState *Storage::FindOrCreate(const CBlockIndex *index) {
  LOCK(cs);
  if (const auto state = Find(index)) {
    return state;
  }
  return Create(index);
}

void Storage::Reset(const esperanza::FinalizationParams &params,
                    const esperanza::AdminParams &admin_params) {
  LOCK(cs);
  m_states.clear();
  m_genesis_state.reset(new FinalizationState(params, admin_params));
}

void Storage::ResetToTip(const esperanza::FinalizationParams &params,
                         const esperanza::AdminParams &admin_params,
                         const CBlockIndex *index) {
  LOCK(cs);
  Reset(params, admin_params);
  m_states.emplace(index, FinalizationState(*Genesis()));
}

void Storage::ClearUntilHeight(blockchain::Height height) {
  LOCK(cs);
  for (auto it = m_states.begin(); it != m_states.end();) {
    const auto index = it->first;
    if (static_cast<blockchain::Height>(index->nHeight) < height) {
      it = m_states.erase(it);
    } else {
      ++it;
    }
  }
}

FinalizationState *Storage::Genesis() const {
  LOCK(cs);
  return m_genesis_state.get();
}

// Global functions section

bool ProcessNewTip(const CBlockIndex &block_index, const CBlock &block) {
  LogPrint(BCLog::FINALIZATION, "%s: Processing block %d with hash %s.\n",
           __func__, block_index.nHeight, block_index.GetBlockHash().GetHex());
  const auto state = g_storage.FindOrCreate(&block_index);
  if (state == nullptr) {
    return false;
  }
  return state->ProcessNewTip(block_index, block);
}

// In this version we read all the blocks from the disk.
// This function might be significantly optimized by using finalization
// state serialization.
void RestoreFinalizationState(const CChainParams &chainparams) {
  Storage::RestoringRAII restoring(g_storage);
  if (fPruneMode) {
    if (chainActive.Tip() != nullptr) {
      FinalizationState::ResetToTip(*chainActive.Tip());
    } else {
      FinalizationState::Reset(chainparams.GetFinalization(), chainparams.GetAdminParams());
    }
    return;
  }
  LogPrint(BCLog::FINALIZATION, "Restore finalization state from disk\n");
  g_storage.Reset(chainparams.GetFinalization(), chainparams.GetAdminParams());
  for (int i = 1; i <= chainActive.Height(); ++i) {
    const CBlockIndex *const index = chainActive[i];
    CBlock block;
    if (!ReadBlockFromDisk(block, index, chainparams.GetConsensus())) {
      assert(not("Failed to read block"));
    }
    esperanza::ProcessNewTip(*index, block);
  }
}

}  // namespace esperanza
