// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_FINALIZATIONSTATE_H
#define UNITE_ESPERANZA_FINALIZATIONSTATE_H

#include <better-enums/enum.h>
#include <chain.h>
#include <esperanza/admincommand.h>
#include <esperanza/adminstate.h>
#include <esperanza/checkpoint.h>
#include <esperanza/finalizationparams.h>
#include <esperanza/validator.h>
#include <primitives/block.h>
#include <sync.h>
#include <ufp64.h>
#include <uint256.h>
#include <utility>

namespace esperanza {

// clang-format off
BETTER_ENUM(
  Result,
  uint8_t,
  SUCCESS,
  INIT_WRONG_EPOCH,
  INIT_INVALID_REWARD,
  DEPOSIT_INSUFFICIENT,
  DEPOSIT_ALREADY_VALIDATOR,
  VOTE_MALFORMED,
  VOTE_NOT_BY_VALIDATOR,
  VOTE_NOT_VOTABLE,
  VOTE_ALREADY_VOTED,
  VOTE_WRONG_TARGET_HASH,
  VOTE_WRONG_TARGET_EPOCH,
  VOTE_SRC_EPOCH_NOT_JUSTIFIED,
  LOGOUT_ALREADY_DONE,
  LOGOUT_NOT_A_VALIDATOR,
  WITHDRAW_BEFORE_END_DYNASTY,
  WITHDRAW_TOO_EARLY,
  WITHDRAW_NOT_A_VALIDATOR,
  WITHDRAW_WRONG_AMOUNT,
  SLASH_SAME_VOTE,
  SLASH_NOT_SAME_VALIDATOR,
  SLASH_TOO_EARLY,
  SLASH_ALREADY_SLASHED,
  SLASH_NOT_VALID,
  SLASH_NOT_A_VALIDATOR,
  ADMIN_BLACKLISTED,
  ADMIN_NOT_AUTHORIZED
)
// clang-format on

/**
 * This class is the base data-class with all the data required by
 * FinalizationState. If you need to add new data member to FinalizationState
 * you probably would add it here.
 */
class FinalizationStateData {
 protected:
  FinalizationStateData(const AdminParams &adminParams);
  FinalizationStateData(const FinalizationStateData &) = default;
  FinalizationStateData(FinalizationStateData &&) = default;
  ~FinalizationStateData() = default;

  /**
   * A quick comment on the types chosen to represent the various class members:
   * uint32_t - is enough to represent any epoch (even with one epoch a second
   * it would last ~136 yrs)
   * uint64_t - is enough to represent any amount of UNIT-E coins
   * (total_supply=(e * 10^17) and log2(total_supply)=~58 )
   * ufp64_t - is a way to represent a decimal number with integer part up to
   * 10E9 and decimal part with precision of 10E-8. Using this type is safe as
   * long as the above conditions are met. For example multiplications between
   * ufp64t and uint64_t are for example safe since for the intermediate step a
   * bigger int type is used, but if the result is not representable by 32 bits
   * then the final value will overflow.
   */

  // Map of epoch number to checkpoint
  std::map<uint32_t, Checkpoint> m_checkpoints;

  // Map of epoch number to dynasty number
  std::map<uint32_t, uint32_t> m_epochToDynasty;

  // Map of epoch number to the starting epoch for that dynasty
  std::map<uint32_t, uint32_t> m_dynastyStartEpoch;

  // Map of epoch number to checkpoint hash
  std::map<uint32_t, uint256> m_epochToCheckpointHash;

  // List of validators
  std::map<uint160, Validator> m_validators;

  // Map of the dynasty number with the delta in deposits with the previous one
  std::map<uint32_t, uint64_t> m_dynastyDeltas;

  // Map of the epoch number with the deposit scale factor
  std::map<uint32_t, ufp64::ufp64_t> m_depositScaleFactor;

  // Map of the epoch number with the running total of deposits slashed
  std::map<uint32_t, uint64_t> m_totalSlashed;

  // Is true if the current expected hash justified
  bool m_mainHashJustified = false;

  // The current epoch number
  uint32_t m_currentEpoch = 0;

  // The current dynasy number
  uint32_t m_currentDynasty = 0;

  // Total scaled deposits in the current dynasty
  uint64_t m_curDynDeposits = 0;

  // Total scaled deposits in the previous dynasty
  uint64_t m_prevDynDeposits = 0;

  // Expected source epoch
  uint32_t m_expectedSrcEpoch = 0;

  // Number of the last finalized epoch
  uint32_t m_lastFinalizedEpoch = 0;

  // Number of the last justified epoch
  uint32_t m_lastJustifiedEpoch = 0;

  // Hash of the last checkpoint
  uint256 m_recommendedTargetHash;

  ufp64::ufp64_t m_lastVoterRescale = 0;

  ufp64::ufp64_t m_lastNonVoterRescale = 0;

  // Reward for voting as fraction of deposit size
  ufp64::ufp64_t m_rewardFactor = 0;

  AdminState m_adminState;
};

/**
 * This class is thread safe, any public method that is actually changing
 * the internal state is guarded against concurrent access.
 */
class FinalizationState : public FinalizationStateData {
 public:
  FinalizationState(const esperanza::FinalizationParams &params,
                    const esperanza::AdminParams &adminParams);
  FinalizationState(const FinalizationState &parent);
  FinalizationState(FinalizationState &&) = default;

  Result InitializeEpoch(int blockHeight);

  Result ValidateDeposit(const uint160 &validatorAddress,
                         CAmount depositValue) const;

  void ProcessDeposit(const uint160 &validatorAddress, CAmount depositValue);

  Result ValidateVote(const Vote &vote) const;
  void ProcessVote(const Vote &vote);

  Result ValidateLogout(const uint160 &validatorAddress) const;
  void ProcessLogout(const uint160 &validatorAddress);

  Result ValidateWithdraw(const uint160 &validatorAddress,
                          CAmount requestedWithdraw) const;

  Result CalculateWithdrawAmount(const uint160 &validatorAddress,
                                 CAmount &withdrawAmountOut) const;

  void ProcessWithdraw(const uint160 &validatorAddress);

  Result IsSlashable(const Vote &vote1, const Vote &vote2) const;
  void ProcessSlash(const Vote &vote1, const Vote &vote2,
                    CAmount &slashingBountyOut);

  bool IsPermissioningActive() const;
  Result ValidateAdminKeys(const AdminKeySet &adminKeys) const;
  void ProcessAdminCommands(const std::vector<AdminCommand> &commands);

  uint32_t GetLastJustifiedEpoch() const;
  uint32_t GetLastFinalizedEpoch() const;

  uint32_t GetCurrentEpoch() const;
  uint32_t GetCurrentDynasty() const;

  uint64_t GetDepositSize(const uint160 &validatorAddress) const;

  Vote GetRecommendedVote(const uint160 &validatorAddress) const;

  std::vector<Validator> GetValidators() const;
  const Validator *GetValidator(const uint160 &validatorAddress) const;

  static void Init(const esperanza::FinalizationParams &params,
                   const esperanza::AdminParams &adminParams);
  static void Reset(const esperanza::FinalizationParams &params,
                    const esperanza::AdminParams &adminParams);

  static FinalizationState *GetState(const CBlockIndex *block = nullptr);

  static uint32_t GetEpoch(const CBlockIndex *blockIndex);

  static bool ValidateDepositAmount(CAmount amount);

  static bool ProcessNewTip(const CBlockIndex &blockIndex, const CBlock &block);

 private:
  void InstaFinalize();
  void IncrementDynasty();
  ufp64::ufp64_t GetCollectiveRewardFactor();
  bool DepositExists();
  ufp64::ufp64_t GetSqrtOfTotalDeposits();
  uint32_t GetEpochsSinceFinalization();
  Result IsVotable(const Validator &validator, const uint256 &targetHash,
                   uint32_t targetEpoch, uint32_t sourceEpoch) const;
  bool IsInDynasty(const Validator &validator, uint32_t dynasty) const;
  CAmount ProcessReward(const uint160 &validatorAddress, uint64_t reward);
  void DeleteValidator(const uint160 &validatorAddress);
  uint64_t GetTotalCurDynDeposits();
  uint64_t GetTotalPrevDynDeposits();
  uint32_t GetEndDynasty() const;
  uint64_t CalculateVoteReward(const Validator &validator) const;
  ufp64::ufp64_t GetDepositScaleFactor(uint32_t epoch) const;
  uint64_t GetTotalSlashed(uint32_t epoch) const;
  Checkpoint &GetCheckpoint(uint32_t epoch);
  uint64_t GetDynastyDelta(uint32_t dynasty);

  mutable CCriticalSection cs_esperanza;

 protected:
  const FinalizationParams &m_settings;

 private:
  void OnBlock(int blockHeight);
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_FINALIZATIONSTATE_H
