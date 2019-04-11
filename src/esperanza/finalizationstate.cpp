// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/finalizationstate.h>

#include <chainparams.h>
#include <esperanza/checks.h>
#include <esperanza/vote.h>
#include <script/ismine.h>
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
const ufp64::ufp64_t BASE_DEPOSIT_SCALE_FACTOR = ufp64::to_ufp64(1);
}  // namespace

template <typename... Args>
inline Result fail(Result error, const char *fmt, const Args &... args) {
  std::string reason = tfm::format(fmt, args...);
  LogPrint(BCLog::FINALIZATION, "ERROR: %s.\n", reason);
  return error;
}

inline Result success() { return Result::SUCCESS; }

FinalizationState::FinalizationState(
    const esperanza::FinalizationParams &params,
    const esperanza::AdminParams &adminParams)
    : FinalizationStateData(adminParams), m_settings(params) {
  m_deposit_scale_factor[0] = BASE_DEPOSIT_SCALE_FACTOR;
  m_total_slashed[0] = 0;
  m_dynasty_deltas[0] = 0;

  Checkpoint cp = Checkpoint();
  cp.m_is_justified = true;
  cp.m_is_finalized = true;
  m_checkpoints[0] = cp;
}

FinalizationState::FinalizationState(const FinalizationState &parent, InitStatus status)
    : FinalizationStateData(parent),
      m_settings(parent.m_settings),
      m_status(status) {}

FinalizationState::FinalizationState(FinalizationState &&parent)
    : FinalizationStateData(std::move(parent)),
      m_settings(parent.m_settings),
      m_status(parent.m_status) {}

bool FinalizationState::operator==(const FinalizationState &other) const {
  return static_cast<const FinalizationStateData &>(*this) ==
         static_cast<const FinalizationStateData &>(other);
}

bool FinalizationState::operator!=(const FinalizationState &other) const {
  return not(*this == other);
}

Result FinalizationState::InitializeEpoch(blockchain::Height blockHeight) {
  LOCK(cs_esperanza);

  assert(IsEpochStart(blockHeight) &&
         "provided blockHeight is not the first block of a new epoch");

  IncrementDynasty();

  const uint32_t new_epoch = GetEpoch(blockHeight);

  if (new_epoch != m_current_epoch + 1) {
    return fail(Result::INIT_WRONG_EPOCH,
                "%s: new_epoch must be %d but %d was passed\n",
                __func__, m_current_epoch + 1, new_epoch);
  }

  LogPrint(BCLog::FINALIZATION, "%s: new_epoch=%d starts at height=%d\n",
           __func__, new_epoch, blockHeight);

  Checkpoint cp = Checkpoint();
  cp.m_cur_dynasty_deposits = GetTotalCurDynDeposits();
  cp.m_prev_dynasty_deposits = GetTotalPrevDynDeposits();
  m_checkpoints[new_epoch] = cp;

  m_current_epoch = new_epoch;

  m_last_voter_rescale = ufp64::add_uint(GetCollectiveRewardFactor(), 1);

  m_last_non_voter_rescale = ufp64::div(m_last_voter_rescale, (ufp64::add_uint(m_reward_factor, 1)));

  m_deposit_scale_factor[new_epoch] = ufp64::mul(m_last_non_voter_rescale, GetDepositScaleFactor(new_epoch - 1));

  m_total_slashed[new_epoch] = GetTotalSlashed(new_epoch - 1);

  if (DepositExists()) {
    ufp64::ufp64_t interestBase = ufp64::div(m_settings.base_interest_factor, GetSqrtOfTotalDeposits());

    m_reward_factor = ufp64::add(interestBase, ufp64::mul_by_uint(m_settings.base_penalty_factor,
                                                                  GetEpochsSinceFinalization()));

    if (m_reward_factor <= 0) {
      return fail(Result::INIT_INVALID_REWARD, "Invalid reward factor %d",
                  m_reward_factor);
    }

  } else {
    InstaJustify();
    m_reward_factor = 0;
  }

  std::string log_msg;
  if (m_current_epoch >= 2 && m_last_justified_epoch != m_current_epoch - 2) {
    log_msg = " epoch=" + std::to_string(m_current_epoch - 2) + " was not justified.";
  }

  LogPrint(BCLog::FINALIZATION,
           "%s:%s new_epoch=%d current_dynasty=%d last_justified_epoch=%d last_finalized_epoch=%d\n",
           __func__,
           log_msg,
           new_epoch,
           m_current_dynasty,
           m_last_justified_epoch,
           m_last_finalized_epoch);

  return success();
}

void FinalizationState::InstaJustify() {
  Checkpoint &cp = GetCheckpoint(m_current_epoch - 1);
  cp.m_is_justified = true;
  m_last_justified_epoch = m_current_epoch - 1;

  if (m_current_epoch > 1) {
    uint32_t expected_finalized = m_current_epoch - 2;
    if (GetCheckpoint(expected_finalized).m_is_justified) {
      GetCheckpoint(expected_finalized).m_is_finalized = true;
      m_last_finalized_epoch = expected_finalized;
    }
  }

  LogPrint(BCLog::FINALIZATION, "%s: Justified epoch=%d.\n", __func__,
           m_last_justified_epoch);
}

void FinalizationState::IncrementDynasty() {
  // finalized epoch is m_current_epoch - 2 because:
  // finalized (0) - justified (1) - votes to justify (2)

  // skip dynasty increment for the hardcoded finalized epoch=0
  // as it's already "considered" incremented from -1 to 0.
  if (m_current_epoch > 2 && GetCheckpoint(m_current_epoch - 2).m_is_finalized) {

    m_current_dynasty += 1;
    m_prev_dyn_deposits = m_cur_dyn_deposits;
    m_cur_dyn_deposits += GetDynastyDelta(m_current_dynasty);
    m_dynasty_start_epoch[m_current_dynasty] = m_current_epoch;

    LogPrint(BCLog::FINALIZATION, "%s: New current dynasty=%d\n", __func__,
             m_current_dynasty);
    // UNIT-E: we can clear old checkpoints (up to m_last_finalized_epoch - 1)
  }
  m_epoch_to_dynasty[m_current_epoch] = m_current_dynasty;
}

ufp64::ufp64_t FinalizationState::GetCollectiveRewardFactor() {
  uint32_t epoch = m_current_epoch;
  bool isLive = GetEpochsSinceFinalization() <= 2;

  if (!DepositExists() || !isLive) {
    return 0;
  }

  ufp64::ufp64_t curVoteFraction = ufp64::div_2uint(
      GetCheckpoint(epoch - 1).GetCurDynastyVotes(m_expected_source_epoch),
      m_cur_dyn_deposits);

  ufp64::ufp64_t prevVoteFraction = ufp64::div_2uint(
      GetCheckpoint(epoch - 1).GetPrevDynastyVotes(m_expected_source_epoch),
      m_prev_dyn_deposits);

  ufp64::ufp64_t voteFraction = ufp64::min(curVoteFraction, prevVoteFraction);

  return ufp64::div_by_uint(ufp64::mul(voteFraction, m_reward_factor), 2);
}

bool FinalizationState::DepositExists() const {
  return m_cur_dyn_deposits > 0;
}

ufp64::ufp64_t FinalizationState::GetSqrtOfTotalDeposits() const {
  uint64_t totalDeposits = 1 + ufp64::mul_to_uint(GetDepositScaleFactor(m_current_epoch - 1),
                                                  std::max(m_prev_dyn_deposits, m_cur_dyn_deposits));

  return ufp64::sqrt_uint(totalDeposits);
}

uint32_t FinalizationState::GetEpochsSinceFinalization() const {
  return m_current_epoch - m_last_finalized_epoch;
}

void FinalizationState::DeleteValidator(const uint160 &validatorAddress) {
  LOCK(cs_esperanza);

  m_validators.erase(validatorAddress);
}

uint64_t FinalizationState::GetDepositSize(const uint160 &validatorAddress) const {
  LOCK(cs_esperanza);

  auto validatorIt = m_validators.find(validatorAddress);
  auto depositScaleIt = m_deposit_scale_factor.find(m_current_epoch);

  if (validatorIt != m_validators.end() &&
      !validatorIt->second.m_is_slashed &&
      depositScaleIt != m_deposit_scale_factor.end()) {

    return ufp64::mul_to_uint(depositScaleIt->second, validatorIt->second.m_deposit);
  } else {
    return 0;
  }
}

uint32_t FinalizationState::GetRecommendedTargetEpoch() const {
  return m_recommended_target_epoch;
}

Vote FinalizationState::GetRecommendedVote(const uint160 &validatorAddress) const {
  LOCK(cs_esperanza);

  Vote vote;
  vote.m_validator_address = validatorAddress;
  vote.m_target_hash = m_recommended_target_hash;
  vote.m_target_epoch = m_recommended_target_epoch;
  vote.m_source_epoch = m_expected_source_epoch;

  LogPrint(BCLog::FINALIZATION,
           "%s: source_epoch=%d target_epoch=%d dynasty=%d target_hash=%s.\n",
           __func__,
           vote.m_source_epoch,
           vote.m_target_epoch,
           m_current_dynasty,
           vote.m_target_hash.GetHex());

  return vote;
}

bool FinalizationState::IsInDynasty(const Validator &validator, uint32_t dynasty) const {

  uint32_t startDynasty = validator.m_start_dynasty;
  uint32_t endDynasty = validator.m_end_dynasty;
  return (startDynasty <= dynasty) && (dynasty < endDynasty);
}

uint64_t FinalizationState::GetTotalCurDynDeposits() const {

  return ufp64::mul_to_uint(GetDepositScaleFactor(m_current_epoch),
                            m_cur_dyn_deposits);
}

uint64_t FinalizationState::GetTotalPrevDynDeposits() const {

  if (m_current_epoch == 0) {
    return 0;
  }

  return ufp64::mul_to_uint(GetDepositScaleFactor(m_current_epoch - 1), m_prev_dyn_deposits);
}

CAmount FinalizationState::ProcessReward(const uint160 &validatorAddress, uint64_t reward) {

  Validator &validator = m_validators.at(validatorAddress);
  validator.m_deposit = validator.m_deposit + reward;
  uint32_t startDynasty = validator.m_start_dynasty;
  uint32_t endDynasty = validator.m_end_dynasty;

  if ((startDynasty <= m_current_dynasty) && (m_current_dynasty < endDynasty)) {
    m_cur_dyn_deposits += reward;
  }

  if ((startDynasty <= m_current_dynasty - 1) && (m_current_dynasty - 1 < endDynasty)) {
    m_prev_dyn_deposits += reward;
  }

  if (endDynasty < DEFAULT_END_DYNASTY) {
    m_dynasty_deltas[endDynasty] = GetDynastyDelta(endDynasty) - reward;
  }

  return ufp64::mul_to_uint(GetDepositScaleFactor(m_current_epoch),
                            validator.m_deposit);

  // UNIT-E: Here is where we should reward proposers if we want
}

Result FinalizationState::IsVotable(const Validator &validator,
                                    const uint256 &targetHash,
                                    uint32_t targetEpoch,
                                    uint32_t sourceEpoch) const {

  auto validatorAddress = validator.m_validator_address;

  auto it = m_checkpoints.find(targetEpoch);
  if (it == m_checkpoints.end()) {
    return fail(Result::VOTE_MALFORMED,
                "%s: target_epoch=%d is in the future.\n", __func__,
                targetEpoch);
  }

  auto &targetCheckpoint = it->second;
  bool alreadyVoted = targetCheckpoint.m_vote_set.find(validatorAddress) !=
                      targetCheckpoint.m_vote_set.end();

  if (alreadyVoted) {
    return fail(Result::VOTE_ALREADY_VOTED,
                "%s: validator=%s has already voted for target_epoch=%d.\n",
                __func__, validatorAddress.GetHex(), targetEpoch);
  }

  if (targetHash != m_recommended_target_hash) {
    return fail(Result::VOTE_WRONG_TARGET_HASH,
                "%s: validator=%s is voting for target=%s instead of the "
                "recommended_target=%s.\n",
                __func__, validatorAddress.GetHex(), targetHash.GetHex(),
                m_recommended_target_hash.GetHex());
  }

  if (targetEpoch != m_current_epoch - 1) {
    return fail(
        Result::VOTE_WRONG_TARGET_EPOCH,
        "%s: vote for wrong target_epoch=%d. validator=%s current_epoch=%d\n",
        __func__, targetEpoch, validatorAddress.GetHex(), m_current_epoch);
  }

  it = m_checkpoints.find(sourceEpoch);
  if (it == m_checkpoints.end()) {
    return fail(Result::VOTE_MALFORMED,
                "%s: source_epoch=%d is in the future. current_epoch=%d\n", __func__,
                sourceEpoch, m_current_epoch);
  }

  auto &sourceCheckpoint = it->second;
  if (!sourceCheckpoint.m_is_justified) {
    return fail(
        Result::VOTE_SRC_EPOCH_NOT_JUSTIFIED,
        "%s: validator=%s is voting for a non justified source epoch=%d.\n",
        __func__, validatorAddress.GetHex(), targetEpoch);
  }

  if (IsFinalizerVoting(validator)) {
    return success();
  }

  return fail(Result::VOTE_NOT_VOTABLE,
              "%s: validator=%s is not in dynasty=%d nor the previous.\n",
              __func__, validatorAddress.GetHex(), m_current_dynasty);
}

Result FinalizationState::ValidateDeposit(const uint160 &validatorAddress,
                                          CAmount depositValue) const {
  LOCK(cs_esperanza);

  if (!m_admin_state.IsValidatorAuthorized(validatorAddress)) {
    return fail(esperanza::Result::ADMIN_BLACKLISTED,
                "%s: validator=%s is blacklisted.\n", __func__,
                validatorAddress.GetHex());
  }

  if (m_validators.find(validatorAddress) != m_validators.end()) {
    return fail(Result::DEPOSIT_DUPLICATE,
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

  uint32_t startDynasty = m_current_dynasty + 3;
  uint64_t scaledDeposit = ufp64::div_to_uint(static_cast<uint64_t>(depositValue),
                                              GetDepositScaleFactor(m_current_epoch));

  m_validators.insert(std::pair<uint160, Validator>(
      validatorAddress,
      Validator(scaledDeposit, startDynasty, validatorAddress)));

  m_dynasty_deltas[startDynasty] = GetDynastyDelta(startDynasty) + scaledDeposit;

  LogPrint(BCLog::FINALIZATION,
           "%s: Add deposit %s for validator in dynasty %d.\n", __func__,
           validatorAddress.GetHex(), startDynasty);
}

uint64_t FinalizationState::CalculateVoteReward(const Validator &validator) const {
  return ufp64::mul_to_uint(m_reward_factor, validator.m_deposit);
}

Result FinalizationState::ValidateVote(const Vote &vote) const {
  LOCK(cs_esperanza);

  if (!m_admin_state.IsValidatorAuthorized(vote.m_validator_address)) {
    return fail(esperanza::Result::ADMIN_BLACKLISTED,
                "%s: validator=%s is blacklisted\n", __func__,
                vote.m_validator_address.GetHex());
  }

  auto it = m_validators.find(vote.m_validator_address);
  if (it == m_validators.end()) {
    return fail(Result::VOTE_NOT_BY_VALIDATOR,
                "%s: No validator with index %s found.\n", __func__,
                vote.m_validator_address.GetHex());
  }

  Result isVotable = IsVotable(it->second, vote.m_target_hash,
                               vote.m_target_epoch, vote.m_source_epoch);

  if (isVotable != +Result::SUCCESS) {
    return fail(isVotable, "%s: not votable. validator=%s target=%s source_epoch=%d target_epoch=%d\n",
                __func__,
                vote.m_validator_address.GetHex(),
                vote.m_target_hash.GetHex(),
                vote.m_source_epoch,
                vote.m_target_epoch);
  }

  LogPrint(BCLog::FINALIZATION,
           "%s: valid. validator=%s target=%s source_epoch=%d target_epoch=%d\n",
           __func__,
           vote.m_validator_address.GetHex(),
           vote.m_target_hash.GetHex(),
           vote.m_source_epoch,
           vote.m_target_epoch);

  return success();
}

void FinalizationState::ProcessVote(const Vote &vote) {
  LOCK(cs_esperanza);

  GetCheckpoint(vote.m_target_epoch).m_vote_set.insert(vote.m_validator_address);

  LogPrint(BCLog::FINALIZATION,
           "%s: validator=%s voted successfully. target=%s source_epoch=%d target_epoch=%d.\n",
           __func__,
           vote.m_validator_address.GetHex(),
           vote.m_target_hash.GetHex(),
           vote.m_source_epoch,
           vote.m_target_epoch);

  const uint160 &validatorAddress = vote.m_validator_address;
  uint32_t sourceEpoch = vote.m_source_epoch;
  uint32_t targetEpoch = vote.m_target_epoch;
  const Validator &validator = m_validators.at(validatorAddress);

  bool inCurDynasty = IsInDynasty(validator, m_current_dynasty);
  bool inPrevDynasty = IsInDynasty(validator, m_current_dynasty - 1);

  uint64_t curDynastyVotes = GetCheckpoint(targetEpoch).GetCurDynastyVotes(sourceEpoch);

  uint64_t prevDynastyVotes = GetCheckpoint(targetEpoch).GetPrevDynastyVotes(sourceEpoch);

  if (inCurDynasty) {
    curDynastyVotes += validator.m_deposit;
    GetCheckpoint(targetEpoch).m_cur_dynasty_votes[sourceEpoch] = curDynastyVotes;
  }

  if (inPrevDynasty) {
    prevDynastyVotes += validator.m_deposit;
    GetCheckpoint(targetEpoch).m_prev_dynasty_votes[sourceEpoch] = prevDynastyVotes;
  }

  if (m_expected_source_epoch == sourceEpoch) {
    uint64_t reward = CalculateVoteReward(validator);
    ProcessReward(validatorAddress, reward);
  }

  bool isTwoThirdsCurDyn =
      curDynastyVotes >= ufp64::div_to_uint(m_cur_dyn_deposits * 2, ufp64::to_ufp64(3));

  bool isTwoThirdsPrevDyn =
      prevDynastyVotes >= ufp64::div_to_uint(m_prev_dyn_deposits * 2, ufp64::to_ufp64(3));

  bool enoughVotes = isTwoThirdsCurDyn && isTwoThirdsPrevDyn;

  if (enoughVotes && !GetCheckpoint(targetEpoch).m_is_justified) {

    GetCheckpoint(targetEpoch).m_is_justified = true;
    m_last_justified_epoch = targetEpoch;

    LogPrint(BCLog::FINALIZATION, "%s: epoch=%d justified.\n", __func__,
             targetEpoch);

    if (targetEpoch == sourceEpoch + 1) {
      GetCheckpoint(sourceEpoch).m_is_finalized = true;
      m_last_finalized_epoch = sourceEpoch;
      LogPrint(BCLog::FINALIZATION, "%s: epoch=%d finalized.\n", __func__,
               sourceEpoch);
    }
  }
  LogPrint(BCLog::FINALIZATION, "%s: vote from validator=%s processed.\n",
           __func__, validatorAddress.GetHex());
}

uint32_t FinalizationState::GetEndDynasty() const {
  return m_current_dynasty + m_settings.dynasty_logout_delay;
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

  if (validator.m_start_dynasty > m_current_dynasty) {
    return fail(Result::LOGOUT_NOT_YET_A_VALIDATOR,
                "%s: the validator with address %s is logging out before the "
                "start dynasty.\n",
                __func__, validator.m_validator_address.GetHex());
  }

  if (validator.m_end_dynasty <= endDynasty) {
    return fail(Result::LOGOUT_ALREADY_DONE,
                "%s: validator=%s already logged out.\n",
                __func__, validator.m_validator_address.GetHex());
  }

  return success();
}

void FinalizationState::ProcessLogout(const uint160 &validatorAddress) {
  LOCK(cs_esperanza);

  Validator &validator = m_validators.at(validatorAddress);

  uint32_t endDyn = GetEndDynasty();
  validator.m_end_dynasty = endDyn;
  validator.m_deposits_at_logout = m_cur_dyn_deposits;
  m_dynasty_deltas[endDyn] = GetDynastyDelta(endDyn) - validator.m_deposit;

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

  uint32_t endDynasty = validator.m_end_dynasty;

  if (m_current_dynasty <= endDynasty) {
    return fail(Result::WITHDRAW_BEFORE_END_DYNASTY,
                "%s: Too early to withdraw, minimum expected dynasty for "
                "withdraw is %d.\n",
                __func__, endDynasty);
  }

  uint32_t endEpoch = m_dynasty_start_epoch.find(endDynasty + 1)->second;
  uint32_t withdrawalEpoch = endEpoch + m_settings.withdrawal_epoch_delay;

  if (m_current_epoch < withdrawalEpoch) {
    return fail(Result::WITHDRAW_TOO_EARLY,
                "%s: Too early to withdraw, minimum expected epoch for "
                "withdraw is %d.\n",
                __func__, withdrawalEpoch);
  }

  if (!validator.m_is_slashed) {
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
                                                      validator.m_deposits_at_logout);

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
  return m_admin_state.IsPermissioningActive();
}

Result FinalizationState::ValidateAdminKeys(const AdminKeySet &adminKeys) const {
  LOCK(cs_esperanza);

  if (m_admin_state.IsAdminAuthorized(adminKeys)) {
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
          m_admin_state.AddValidator(pubkey.GetID());
        }
        break;
      }
      case AdminCommandType::REMOVE_FROM_WHITELIST: {
        for (const auto &pubkey : command.GetPayload()) {
          m_admin_state.RemoveValidator(pubkey.GetID());
        }
        break;
      }
      case AdminCommandType::RESET_ADMINS: {
        const auto &pubkeys = command.GetPayload();
        AdminKeySet set;
        std::copy_n(pubkeys.begin(), ADMIN_MULTISIG_KEYS, set.begin());
        m_admin_state.ResetAdmin(set);
        break;
      }
      case AdminCommandType::END_PERMISSIONING: {
        m_admin_state.EndPermissioning();
        break;
      }
    }
  }
}

Result FinalizationState::IsSlashable(const Vote &vote1,
                                      const Vote &vote2) const {
  LOCK(cs_esperanza);

  auto it = m_validators.find(vote1.m_validator_address);
  if (it == m_validators.end()) {
    return fail(Result::SLASH_NOT_A_VALIDATOR,
                "%s: No validator with index %s found.\n", __func__,
                vote1.m_validator_address.GetHex());
  }
  const Validator &validator1 = it->second;

  it = m_validators.find(vote2.m_validator_address);
  if (it == m_validators.end()) {
    return fail(Result::SLASH_NOT_A_VALIDATOR,
                "%s: No validator with index %s found.\n", __func__,
                vote2.m_validator_address.GetHex());
  }
  const Validator &validator2 = it->second;

  uint160 validatorAddress1 = validator1.m_validator_address;
  uint160 validatorAddress2 = validator2.m_validator_address;

  uint32_t sourceEpoch1 = vote1.m_source_epoch;
  uint32_t targetEpoch1 = vote1.m_target_epoch;

  uint32_t sourceEpoch2 = vote2.m_source_epoch;
  uint32_t targetEpoch2 = vote2.m_target_epoch;

  if (validatorAddress1 != validatorAddress2) {
    return fail(Result::SLASH_NOT_SAME_VALIDATOR,
                "%s: votes have not be casted by the same validator.\n",
                __func__);
  }

  if (validator1.m_start_dynasty > m_current_dynasty) {
    return fail(Result::SLASH_TOO_EARLY,
                "%s: validator with deposit hash %s is not yet voting.\n",
                __func__, vote1.m_validator_address.GetHex());
  }

  if (validator1.m_is_slashed) {
    return fail(
        Result::SLASH_ALREADY_SLASHED,
        "%s: validator with deposit hash %s has been already slashed.\n",
        __func__, vote1.m_validator_address.GetHex());
  }

  if (vote1.m_target_hash == vote2.m_target_hash) {
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

  const uint160 &validatorAddress = vote1.m_validator_address;

  const CAmount validatorDeposit = GetDepositSize(validatorAddress);

  m_total_slashed[m_current_epoch] =
      GetTotalSlashed(m_current_epoch) + validatorDeposit;

  m_validators.at(validatorAddress).m_is_slashed = true;

  LogPrint(BCLog::FINALIZATION,
           "%s: Slashing validator with deposit hash %s of %d units.\n",
           __func__, validatorAddress.GetHex(), validatorDeposit);

  const uint32_t endDynasty = m_validators.at(validatorAddress).m_end_dynasty;

  // if validator not logged out yet, remove total from next dynasty
  // and forcibly logout next dynasty
  if (m_current_dynasty < endDynasty) {
    const CAmount deposit = m_validators.at(validatorAddress).m_deposit;
    m_dynasty_deltas[m_current_dynasty + 1] =
        GetDynastyDelta(m_current_dynasty + 1) - deposit;
    m_validators.at(validatorAddress).m_end_dynasty = m_current_dynasty + 1;

    // if validator was already staged for logout at end_dynasty,
    // ensure that we don't doubly remove from total
    if (endDynasty < DEFAULT_END_DYNASTY) {
      m_dynasty_deltas[endDynasty] = GetDynastyDelta(endDynasty) + deposit;
    } else {
      // if no previously logged out, remember the total deposits at logout
      m_validators.at(validatorAddress).m_deposits_at_logout =
          GetTotalCurDynDeposits();
    }
  }
}

uint32_t FinalizationState::GetCurrentEpoch() const { return m_current_epoch; }

uint32_t FinalizationState::GetLastJustifiedEpoch() const {
  return m_last_justified_epoch;
}

uint32_t FinalizationState::GetLastFinalizedEpoch() const {
  return m_last_finalized_epoch;
}

uint32_t FinalizationState::GetCurrentDynasty() const {
  return m_current_dynasty;
}

uint32_t FinalizationState::GetCheckpointHeightAfterFinalizedEpoch() const {
  const uint32_t epoch = m_last_finalized_epoch + 1;
  if (m_last_finalized_epoch != 0) {
    // epoch=0 is self-finalized and doesn't require
    // parent epoch to justify it but for other epochs
    // this rule must hold
    assert(GetCheckpoint(epoch).m_is_justified);
  }
  return GetEpochCheckpointHeight(epoch);
}

uint32_t FinalizationState::GetEpochLength() const {
  return m_settings.epoch_length;
}

uint32_t FinalizationState::GetEpoch(const CBlockIndex &blockIndex) const {
  return GetEpoch(blockIndex.nHeight);
}

uint32_t FinalizationState::GetEpoch(const blockchain::Height block_height) const {
  uint32_t epoch = block_height / m_settings.epoch_length;
  if (block_height % m_settings.epoch_length != 0) {
    ++epoch;
  }
  return epoch;
}

blockchain::Height FinalizationState::GetEpochStartHeight(const uint32_t epoch) const {
  return m_settings.GetEpochStartHeight(epoch);
}

blockchain::Height FinalizationState::GetEpochCheckpointHeight(const uint32_t epoch) const {
  return m_settings.GetEpochCheckpointHeight(epoch);
}

std::vector<Validator> FinalizationState::GetActiveFinalizers() const {
  std::vector<Validator> res;
  for (const auto &it : m_validators) {
    if (IsFinalizerVoting(it.second)) {
      res.push_back(it.second);
    }
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

bool FinalizationState::ValidateDepositAmount(CAmount amount) const {
  return amount >= m_settings.min_deposit_size;
}

void FinalizationState::ProcessNewCommit(const CTransactionRef &tx) {
  AssertLockHeld(cs_esperanza);
  switch (tx->GetType()) {
    case TxType::VOTE: {
      Vote vote;
      std::vector<unsigned char> voteSig;
      const bool ok = CScript::ExtractVoteFromVoteSignature(tx->vin[0].scriptSig, vote, voteSig);
      assert(ok);
      ProcessVote(vote);
      RegisterLastTx(vote.m_validator_address, tx);
      break;
    }

    case TxType::DEPOSIT: {
      uint160 validatorAddress = uint160();

      const bool ok = ExtractValidatorAddress(*tx, validatorAddress);
      assert(ok);
      ProcessDeposit(validatorAddress, tx->vout[0].nValue);
      RegisterLastTx(validatorAddress, tx);
      break;
    }

    case TxType::LOGOUT: {
      uint160 validatorAddress = uint160();

      const bool ok = ExtractValidatorAddress(*tx, validatorAddress);
      assert(ok);
      ProcessLogout(validatorAddress);
      RegisterLastTx(validatorAddress, tx);
      break;
    }

    case TxType::WITHDRAW: {
      uint160 validatorAddress = uint160();

      const bool ok = ExtractValidatorAddress(*tx, validatorAddress);
      assert(ok);
      ProcessWithdraw(validatorAddress);
      break;
    }

    case TxType::SLASH: {

      esperanza::Vote vote1;
      esperanza::Vote vote2;
      std::vector<unsigned char> voteSig1;
      std::vector<unsigned char> voteSig2;
      const bool ok = CScript::ExtractVotesFromSlashSignature(
          tx->vin[0].scriptSig, vote1, vote2, voteSig1, voteSig2);
      assert(ok);

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
        const bool ok = DecodeAdminCommand(output.scriptPubKey, command);
        assert(ok);

        commands.emplace_back(std::move(command));
      }

      ProcessAdminCommands(commands);
      break;
    }

    case TxType::COINBASE:
    case TxType::REGULAR:
      break;
  }
}

void FinalizationState::ProcessNewTip(const CBlockIndex &block_index,
                                      const CBlock &block) {
  LOCK(cs_esperanza);
  assert(m_status == NEW);
  ProcessNewCommits(block_index, block.vtx);
  m_status = COMPLETED;
}

void FinalizationState::ProcessNewCommits(const CBlockIndex &block_index,
                                          const std::vector<CTransactionRef> &txes) {
  LOCK(cs_esperanza);
  assert(m_status == NEW);
  uint256 block_hash = block_index.GetBlockHash();

  if (IsEpochStart(block_index.nHeight)) {
    InitializeEpoch(block_index.nHeight);
  }

  for (const auto &tx : txes) {
    ProcessNewCommit(tx);
  }

  if (IsCheckpoint(block_index.nHeight)) {
    LogPrint(BCLog::FINALIZATION,
             "%s: Last block of the epoch, new m_recommended_target_hash=%s\n",
             __func__, block_hash.GetHex());

    m_recommended_target_hash = block_index.GetBlockHash();
    m_recommended_target_epoch = GetEpoch(block_index);
    m_expected_source_epoch = m_last_justified_epoch;
  }
  m_status = FROM_COMMITS;
}

// Private accessors used to avoid map's operator[] potential side effects.
ufp64::ufp64_t FinalizationState::GetDepositScaleFactor(uint32_t epoch) const {
  const auto it = m_deposit_scale_factor.find(epoch);
  assert(it != m_deposit_scale_factor.end());
  return it->second;
}

uint64_t FinalizationState::GetTotalSlashed(uint32_t epoch) const {
  const auto it = m_total_slashed.find(epoch);
  assert(it != m_total_slashed.end());
  return it->second;
}

CAmount FinalizationState::GetDynastyDelta(uint32_t dynasty) {
  const auto pair = m_dynasty_deltas.emplace(dynasty, 0);
  return pair.first->second;
}

Checkpoint &FinalizationState::GetCheckpoint(uint32_t epoch) {
  const auto it = m_checkpoints.find(epoch);
  assert(it != m_checkpoints.end());
  return it->second;
}

const Checkpoint &FinalizationState::GetCheckpoint(const uint32_t epoch) const {
  const auto it = m_checkpoints.find(epoch);
  assert(it != m_checkpoints.end());
  return it->second;
}

void FinalizationState::RegisterLastTx(uint160 &validatorAddress,
                                       CTransactionRef tx) {

  Validator &validator = m_validators.at(validatorAddress);
  validator.m_last_transaction_hash = tx->GetHash();
}

uint256 FinalizationState::GetLastTxHash(const uint160 &validatorAddress) const {
  const Validator &validator = m_validators.at(validatorAddress);
  return validator.m_last_transaction_hash;
}

bool FinalizationState::IsEpochStart(blockchain::Height block_height) const {
  return block_height % m_settings.epoch_length == 1;
}

bool FinalizationState::IsCheckpoint(blockchain::Height blockHeight) const {
  return blockHeight % m_settings.epoch_length == 0;
}

bool FinalizationState::IsJustifiedCheckpoint(blockchain::Height blockHeight) const {
  if (!IsCheckpoint(blockHeight)) {
    return false;
  }
  auto const it = m_checkpoints.find(GetEpoch(blockHeight));
  return it != m_checkpoints.end() && it->second.m_is_justified;
}

bool FinalizationState::IsFinalizedCheckpoint(blockchain::Height blockHeight) const {
  if (!IsCheckpoint(blockHeight)) {
    return false;
  }
  auto const it = m_checkpoints.find(GetEpoch(blockHeight));
  return it != m_checkpoints.end() && it->second.m_is_finalized;
}

FinalizationState::InitStatus FinalizationState::GetInitStatus() const {
  return m_status;
}

bool FinalizationState::IsFinalizerVoting(const uint160 &finalizer_address) const {
  const esperanza::Validator *finalizer = GetValidator(finalizer_address);
  if (!finalizer) {
    return false;
  }

  return IsFinalizerVoting(*finalizer);
}

bool FinalizationState::IsFinalizerVoting(const Validator &finalizer) const {
  return IsInDynasty(finalizer, m_current_dynasty) || IsInDynasty(finalizer, m_current_dynasty - 1);
}

}  // namespace esperanza
