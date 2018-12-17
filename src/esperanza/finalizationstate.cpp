// Copyright (c) 2018 The Unit-e developers
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
  LogPrint(BCLog::FINALIZATION, "ERROR: %s.\n", reason);
  return error;
}

Result success() { return Result::SUCCESS; }

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

Result FinalizationState::InitializeEpoch(int blockHeight) {
  LOCK(cs_esperanza);
  auto newEpoch = static_cast<uint32_t>(blockHeight) / m_settings.m_epochLength;

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
    ufp64::ufp64_t interestBase = ufp64::div(m_settings.m_baseInterestFactor, GetSqrtOfTotalDeposits());

    m_rewardFactor = ufp64::add(interestBase, ufp64::mul_by_uint(m_settings.m_basePenaltyFactor,
                                                                 GetEpochsSinceFinalization()));

    if (m_rewardFactor <= 0) {
      return fail(Result::INIT_INVALID_REWARD, "Invalid reward factor %d",
                  m_rewardFactor);
    }

  } else {
    InstaFinalize();
    m_rewardFactor = 0;
  }

  m_epochToCheckpointHash[m_currentEpoch] = m_recommendedTargetHash;

  IncrementDynasty();

  LogPrint(BCLog::FINALIZATION,
           "%s: Epoch with height %d initialized. The current dynasty is %s.\n",
           __func__, newEpoch, m_currentDynasty);

  return success();
}

void FinalizationState::InstaFinalize() {
  uint32_t epoch = this->m_currentEpoch;
  m_mainHashJustified = true;

  Checkpoint &cp = GetCheckpoint(epoch - 1);
  cp.m_isJustified = true;
  cp.m_isFinalized = true;
  m_lastJustifiedEpoch = epoch - 1;
  m_lastFinalizedEpoch = epoch - 1;

  LogPrint(BCLog::FINALIZATION, "%s: Finalized block for epoch %d.\n", __func__,
           epoch);
}

void FinalizationState::IncrementDynasty() {
  uint32_t epoch = this->m_currentEpoch;

  if (epoch > 1 && GetCheckpoint(epoch - 2).m_isFinalized) {

    m_currentDynasty += 1;
    m_prevDynDeposits = m_curDynDeposits;
    m_curDynDeposits += GetDynastyDelta(m_currentDynasty);
    m_dynastyStartEpoch[m_currentDynasty] = epoch;

    LogPrint(BCLog::FINALIZATION, "%s: New current dynasty is %d.\n", __func__,
             m_currentDynasty);
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
      GetCheckpoint(epoch - 1).GetCurDynastyVotes(m_expectedSrcEpoch),
      m_curDynDeposits);

  ufp64::ufp64_t prevVoteFraction = ufp64::div_2uint(
      GetCheckpoint(epoch - 1).GetPrevDynastyVotes(m_expectedSrcEpoch),
      m_prevDynDeposits);

  ufp64::ufp64_t voteFraction = ufp64::min(curVoteFraction, prevVoteFraction);

  return ufp64::div_by_uint(ufp64::mul(voteFraction, m_rewardFactor), 2);
}

bool FinalizationState::DepositExists() {
  return m_curDynDeposits > 0 && m_prevDynDeposits > 0;
}

ufp64::ufp64_t FinalizationState::GetSqrtOfTotalDeposits() {
  uint64_t totalDeposits = 1 + ufp64::mul_to_uint(GetDepositScaleFactor(m_currentEpoch - 1),
                                                  std::max(m_prevDynDeposits, m_curDynDeposits));

  return ufp64::sqrt_uint(totalDeposits);
}

uint32_t FinalizationState::GetEpochsSinceFinalization() {
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

Vote FinalizationState::GetRecommendedVote(const uint160 &validatorAddress) const {
  LOCK(cs_esperanza);

  Vote vote;
  vote.m_validatorAddress = validatorAddress;
  vote.m_targetHash = m_recommendedTargetHash;
  vote.m_targetEpoch = m_currentEpoch;
  vote.m_sourceEpoch = m_expectedSrcEpoch;

  LogPrint(BCLog::FINALIZATION,
           "%s: Getting recommended vote for epoch %d and dynasty %d is: { %s, "
           "%d, %d }.\n",
           __func__, m_currentEpoch, m_currentDynasty,
           m_recommendedTargetHash.GetHex(), m_currentEpoch,
           m_expectedSrcEpoch);

  return vote;
}

bool FinalizationState::IsInDynasty(const Validator &validator, uint32_t dynasty) const {

  uint32_t startDynasty = validator.m_startDynasty;
  uint32_t endDynasty = validator.m_endDynasty;
  return (startDynasty <= dynasty) && (dynasty < endDynasty);
}

uint64_t FinalizationState::GetTotalCurDynDeposits() {

  return ufp64::mul_to_uint(GetDepositScaleFactor(m_currentEpoch),
                            m_curDynDeposits);
}

uint64_t FinalizationState::GetTotalPrevDynDeposits() {

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
                "%s: the target epoch %d is in the future.\n", __func__,
                targetEpoch);
  }

  auto &targetCheckpoint = it->second;
  bool alreadyVoted = targetCheckpoint.m_voteSet.find(validatorAddress) !=
                      targetCheckpoint.m_voteSet.end();

  if (alreadyVoted) {
    return fail(Result::VOTE_ALREADY_VOTED,
                "%s: the validator %s has already voted for target epoch %d.\n",
                __func__, validatorAddress.GetHex(), targetEpoch);
  }

  if (targetHash != m_recommendedTargetHash) {
    return fail(Result::VOTE_WRONG_TARGET_HASH,
                "%s: the validator %s is voting for the %s, instead of the "
                "recommended targetHash %s.\n",
                __func__, validatorAddress.GetHex(), targetHash.GetHex(),
                m_recommendedTargetHash.GetHex());
  }

  if (targetEpoch != m_currentEpoch) {
    return fail(
        Result::VOTE_WRONG_TARGET_EPOCH,
        "%s: the validator %s is voting for the wrong target epoch %d.\n",
        __func__, validatorAddress.GetHex(), targetEpoch);
  }

  it = m_checkpoints.find(sourceEpoch);
  if (it == m_checkpoints.end()) {
    return fail(Result::VOTE_MALFORMED,
                "%s: the source epoch %d is in the future.\n", __func__,
                sourceEpoch);
  }

  auto &sourceCheckpoint = it->second;
  if (!sourceCheckpoint.m_isJustified) {
    return fail(
        Result::VOTE_SRC_EPOCH_NOT_JUSTIFIED,
        "%s: the validator %s is voting for a non justified source epoch %d.\n",
        __func__, validatorAddress.GetHex(), targetEpoch);
  }

  if (IsInDynasty(validator, m_currentDynasty) || IsInDynasty(validator, m_currentDynasty - 1)) {
    return success();
  }

  return fail(Result::VOTE_NOT_VOTABLE,
              "%s: validator %s is not in dynasty %d nor the previous.\n",
              __func__, validatorAddress.GetHex(), m_currentDynasty);
}

Result FinalizationState::ValidateDeposit(const uint160 &validatorAddress,
                                          CAmount depositValue) const {
  LOCK(cs_esperanza);

  if (!m_adminState.IsValidatorAuthorized(validatorAddress)) {
    return fail(esperanza::Result::ADMIN_BLACKLISTED,
                "%s: Validator is blacklisted: %s.\n", __func__,
                validatorAddress.GetHex());
  }

  if (m_validators.find(validatorAddress) != m_validators.end()) {
    return fail(Result::DEPOSIT_ALREADY_VALIDATOR,
                "%s: Validator with deposit hash of %s already "
                "exists.\n",
                __func__, validatorAddress.GetHex());
  }

  if (depositValue < m_settings.m_minDepositSize) {
    return fail(Result::DEPOSIT_INSUFFICIENT,
                "%s: The deposit value must be %d > %d.\n", __func__,
                depositValue, m_settings.m_minDepositSize);
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
                "%s: Validator is blacklisted: %s.\n", __func__,
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
    return fail(isVotable, "%s: The tuple (%s, %s, %d, %d) is not votable.\n",
                __func__, vote.m_validatorAddress.GetHex(),
                vote.m_targetHash.GetHex(), vote.m_sourceEpoch,
                vote.m_targetEpoch);
  }

  LogPrint(BCLog::FINALIZATION,
           "%s: Validator %s vote (%s, %d, %d) is valid.\n", __func__,
           vote.m_validatorAddress.GetHex(), vote.m_targetHash.GetHex(),
           vote.m_sourceEpoch, vote.m_targetEpoch);

  return success();
}

void FinalizationState::ProcessVote(const Vote &vote) {
  LOCK(cs_esperanza);

  GetCheckpoint(vote.m_targetEpoch).m_voteSet.insert(vote.m_validatorAddress);

  LogPrint(BCLog::FINALIZATION,
           "%s: Validator %s voted successfully (%s, %d, %d).\n", __func__,
           vote.m_validatorAddress.GetHex(), vote.m_targetHash.GetHex(),
           vote.m_sourceEpoch, vote.m_targetEpoch);

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

  if (m_expectedSrcEpoch == sourceEpoch) {
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
    m_mainHashJustified = true;

    LogPrint(BCLog::FINALIZATION, "%s: Epoch %d justified.\n", __func__,
             targetEpoch);

    if (targetEpoch == sourceEpoch + 1) {
      GetCheckpoint(sourceEpoch).m_isFinalized = true;
      m_lastFinalizedEpoch = sourceEpoch;
      LogPrint(BCLog::FINALIZATION, "%s: Epoch %d finalized.\n", __func__,
               sourceEpoch);
    }
  }
  LogPrint(BCLog::FINALIZATION, "%s: Vote from validator %s processed.\n",
           __func__, validatorAddress.GetHex());
}

uint32_t FinalizationState::GetEndDynasty() const {
  return m_currentDynasty + m_settings.m_dynastyLogoutDelay;
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
                "%s: the validator with address %s already logget out.\n",
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
           "%s: Vote from validator %s logging out at %d.\n", __func__,
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
  uint32_t withdrawalEpoch = endEpoch + m_settings.m_withdrawalEpochDelay;

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
    if (2 * m_settings.m_withdrawalEpochDelay > withdrawalEpoch) {
      baseEpoch = 0;
    } else {
      baseEpoch = withdrawalEpoch - 2 * m_settings.m_withdrawalEpochDelay;
    }

    uint64_t recentlySlashed = GetTotalSlashed(withdrawalEpoch) - GetTotalSlashed(baseEpoch);

    ufp64::ufp64_t fractionToSlash = ufp64::div_2uint(recentlySlashed * m_settings.m_slashFractionMultiplier,
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

void FinalizationState::OnBlock(int blockHeight) {
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

FinalizationState *FinalizationState::GetState(const CBlockIndex *blockIndex) {
  // UNIT-E: Replace the single instance with a map<block,state> to allow for
  // reorganizations.
  return esperanzaState.get();
}

uint32_t FinalizationState::GetEpoch(const CBlockIndex *blockIndex) {
  return GetEpoch(blockIndex->nHeight);
}

uint32_t FinalizationState::GetEpoch(int blockHeight) {
  return static_cast<uint32_t>(blockHeight) / GetState()->m_settings.m_epochLength;
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
  return amount >= GetState()->m_settings.m_minDepositSize;
}

void FinalizationState::Init(const esperanza::FinalizationParams &params,
                             const esperanza::AdminParams &adminParams) {
  LOCK(cs_init_lock);

  if (!esperanzaState) {
    esperanzaState = std::make_shared<FinalizationState>(params, adminParams);
  }
}

void FinalizationState::Reset(const esperanza::FinalizationParams &params,
                              const esperanza::AdminParams &adminParams) {
  LOCK(cs_init_lock);
  esperanzaState = std::make_shared<FinalizationState>(params, adminParams);
}

bool FinalizationState::ProcessNewTip(const CBlockIndex &blockIndex,
                                      const CBlock &block) {

  FinalizationState *state = GetState(&blockIndex);

  LogPrint(BCLog::FINALIZATION, "%s: Processing block %d with hash %s.\n",
           __func__, blockIndex.nHeight, block.GetHash().GetHex());

  // Used to apply hardcoded parameters for a given block
  state->OnBlock(blockIndex.nHeight);

  // We can skip everything for the genesis block since it isn't suppose to
  // contain esperanza's transactions.
  if (blockIndex.nHeight == 0) {
    return true;
  }

  // This is the first block of a new epoch.
  if (blockIndex.nHeight % state->m_settings.m_epochLength == 0) {
    state->InitializeEpoch(blockIndex.nHeight);
  }

  for (const auto &tx : block.vtx) {
    switch (tx->GetType()) {

      case TxType::VOTE: {
        Vote vote;
        std::vector<unsigned char> voteSig;
        assert(CScript::ExtractVoteFromVoteSignature(tx->vin[0].scriptSig, vote, voteSig));
        state->ProcessVote(vote);
        state->RegisterValidatorTx(vote.m_validatorAddress, tx);
        break;
      }

      case TxType::DEPOSIT: {
        uint160 validatorAddress = uint160();

        assert(ExtractValidatorAddress(*tx, validatorAddress));
        state->ProcessDeposit(validatorAddress, tx->GetValueOut());
        state->RegisterValidatorTx(validatorAddress, tx);
        break;
      }

      case TxType::LOGOUT: {
        uint160 validatorAddress = uint160();

        assert(ExtractValidatorAddress(*tx, validatorAddress));
        state->ProcessLogout(validatorAddress);
        state->RegisterValidatorTx(validatorAddress, tx);
        break;
      }

      case TxType::WITHDRAW: {
        uint160 validatorAddress = uint160();

        assert(ExtractValidatorAddress(*tx, validatorAddress));
        state->ProcessWithdraw(validatorAddress);
        break;
      }

      case TxType::SLASH: {

        esperanza::Vote vote1;
        esperanza::Vote vote2;
        std::vector<unsigned char> voteSig1;
        std::vector<unsigned char> voteSig2;
        CScript::ExtractVotesFromSlashSignature(tx->vin[0].scriptSig, vote1,
                                                vote2, voteSig1, voteSig2);

        state->ProcessSlash(vote1, vote2);
        break;
      }

      case TxType::ADMIN: {
        std::vector<AdminCommand> commands;
        for (const auto &output : tx->vout) {
          AdminCommand command;
          if (!MatchAdminCommand(output.scriptPubKey)) {
            continue;
          }
          DecodeAdminCommand(output.scriptPubKey, command);
          commands.emplace_back(std::move(command));
        }

        state->ProcessAdminCommands(commands);
        break;
      }

      default: {
        break;
      }
    }
  }

  if ((blockIndex.nHeight + 2) % state->m_settings.m_epochLength == 0) {
    // Generate the snapshot for the block which is one block behind the last one.
    // The last epoch block will contain the snapshot hash pointing to this snapshot.
    snapshot::Creator::GenerateOrSkip(state->m_currentEpoch);
  }

  // This is the last block for the current epoch and it represent it, so we
  // update the targetHash.
  if (blockIndex.nHeight % state->m_settings.m_epochLength == state->m_settings.m_epochLength - 1) {
    LogPrint(
        BCLog::FINALIZATION,
        "%s: Last block of the epoch, the new recommended targetHash is %s.\n",
        __func__, block.GetHash().GetHex());

    state->m_recommendedTargetHash = block.GetHash();

    // mark snapshots finalized up to the last finalized block
    int64_t height = (state->m_lastFinalizedEpoch + 1) * state->m_settings.m_epochLength - 1;
    if (height == blockIndex.nHeight) {  // instant confirmation
      snapshot::Creator::FinalizeSnapshots(&blockIndex);
    } else {
      snapshot::Creator::FinalizeSnapshots(chainActive[height]);
    }
  }

  return true;
}

// Private accessors used to avoid map's operator[] potential side effects.
ufp64::ufp64_t FinalizationState::GetDepositScaleFactor(uint32_t epoch) const {
  auto it = m_depositScaleFactor.find(epoch);
  assert(it != m_depositScaleFactor.end());
  return it->second;
}

uint64_t FinalizationState::GetTotalSlashed(uint32_t epoch) const {
  auto it = m_totalSlashed.find(epoch);
  assert(it != m_totalSlashed.end());
  return it->second;
}

uint64_t FinalizationState::GetDynastyDelta(uint32_t dynasty) {
  auto pair = m_dynastyDeltas.emplace(dynasty, 0);
  return pair.first->second;
}

Checkpoint &FinalizationState::GetCheckpoint(uint32_t epoch) {
  auto it = m_checkpoints.find(epoch);
  assert(it != m_checkpoints.end());
  return it->second;
}

void FinalizationState::RegisterValidatorTx(uint160 &validatorAddress,
                                            CTransactionRef tx) {

  Validator &validator = m_validators.at(validatorAddress);
  validator.m_lastTransactionHash = tx->GetHash();
}

uint256 FinalizationState::GetLastTxHash(uint160 &validatorAddress) const {
  const Validator &validator = m_validators.at(validatorAddress);
  return validator.m_lastTransactionHash;
}

bool FinalizationState::IsCheckpoint(int blockHeight) const {
  return blockHeight % m_settings.m_epochLength == 0;
}

bool FinalizationState::IsJustifiedCheckpoint(int blockHeight) const {
  if (!IsCheckpoint(blockHeight)) {
    return false;
  }
  auto it = m_checkpoints.find(GetEpoch(blockHeight));
  return it != m_checkpoints.end() && it->second.m_isJustified;
}

bool FinalizationState::IsFinalizedCheckpoint(int blockHeight) const {
  if (!IsCheckpoint(blockHeight)) {
    return false;
  }
  auto it = m_checkpoints.find(GetEpoch(blockHeight));
  return it != m_checkpoints.end() && it->second.m_isFinalized;
}

}  // namespace esperanza
