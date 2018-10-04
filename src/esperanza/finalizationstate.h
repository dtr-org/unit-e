// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_FINALIZATIONSTATE_H
#define UNITE_ESPERANZA_FINALIZATIONSTATE_H

#include <better-enums/enum.h>
#include <chain.h>
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
  SLASH_SAME_VOTE,
  SLASH_NOT_SAME_VALIDATOR,
  SLASH_TOO_EARLY,
  SLASH_ALREADY_SLASHED,
  SLASH_NOT_VALID,
  SLASH_NOT_A_VALIDATOR,
  WITHDRAW_WRONG_AMOUNT
)
// clang-format on

/**
 * This class is NOT thread safe, any public method that is actually changing
 * the internal state should be guarded against concurrent access.
 */
class FinalizationState {
 public:
  FinalizationState(const esperanza::FinalizationParams &params);

  Result InitializeEpoch(int blockHeight);

  Result ValidateDeposit(const uint256 &validatorIndex,
                         const CAmount &depositValue) const;
  void ProcessDeposit(const uint256 &validatorIndex,
                      const CAmount &depositValue);

  Result ValidateVote(const Vote &vote) const;
  void ProcessVote(const Vote &vote);

  Result ValidateLogout(const uint256 &validatorIndex) const;
  void ProcessLogout(const uint256 &validatorIndex);

  Result ValidateWithdraw(const uint256 &validatorIndex,
                          const CAmount &requiredWithdraw) const;
  void ProcessWithdraw(const uint256 &validatorIndex);

  Result IsSlashable(const Vote &vote1, const Vote &vote2) const;
  void ProcessSlash(const Vote &vote1, const Vote &vote2,
                    CAmount &slashingBountyOut);

  uint32_t GetLastJustifiedEpoch() const;
  uint32_t GetLastFinalizedEpoch() const;

  uint32_t GetCurrentEpoch() const;
  uint32_t GetCurrentDynasty() const;

  uint64_t GetDepositSize(const uint256 &validatorIndex) const;

  Vote GetRecommendedVote(const uint256 &validatorIndex) const;

  std::vector<Validator> GetValidators() const;
  const Validator *GetValidator(const uint256 &validatorIndex) const;

  static void Init(const esperanza::FinalizationParams &params);
  static void Reset(const esperanza::FinalizationParams &params);

  static FinalizationState *GetState(const CBlockIndex *block = nullptr);

  static uint32_t GetEpoch(const CBlockIndex *blockIndex);

  static bool ValidateDepositAmount(CAmount amount);

  static bool ProcessNewTip(const CBlockIndex &blockIndex, const CBlock &block);

 protected:
  mutable CCriticalSection cs_esperanza;

  /**
   * A quick comment on the types chosen to represent the various class members:
   * uint32_t - is enough to represent any epoch (even with one epoch a second
   * it would last ~136 yrs) uint64_t - is enough to represent any amount of
   * UNIT-E coins (total_supply=(e * 10^17) and log2(total_supply)=~58 ) ufp64_t
   * - is a way to represent a decimal number with integer part up to 10E9 and
   * decimal part with precision of 10E-8. Using this type is safe as long as
   * the above conditions are met. For example multiplications between ufp64t
   * and uint64_t are for example safe since for the intermediate step a bigger
   * int type is used, but if the result is not representable by 32 bits then
   * the final value will overflow.
   */

  // Map of epoch number to checkpoint
  std::map<uint64_t, Checkpoint> m_checkpoints;

  // Map of epoch number to dynasty number
  std::map<uint32_t, uint32_t> m_epochToDynasty;

  // Map of epoch number to the starting epoch for that dynasty
  std::map<uint32_t, uint32_t> m_dynastyStartEpoch;

  // Map of epoch number to checkpoint hash
  std::map<uint32_t, uint256> m_epochToCheckpointHash;

  // List of validators
  std::map<uint256, Validator> m_validators;

  // Map of the dynasty number with the delta in deposits with the previous one
  std::map<uint32_t, uint64_t> m_dynastyDeltas;

  // Map of the epoch number with the deposit scale factor
  std::map<uint32_t, ufp64::ufp64_t> m_depositScaleFactor;

  // Running total of deposits slashed
  std::map<uint32_t, uint64_t> m_totalSlashed;

  // Is the current expected hash justified
  bool m_mainHashJustified;

  // the current epoch number
  uint32_t m_currentEpoch;

  // the current dynasy number
  uint32_t m_currentDynasty;

  // Total scaled deposits in the current dynasty
  uint64_t m_curDynDeposits;

  // Total scaled deposits in the previous dynasty
  uint64_t m_prevDynDeposits;

  uint32_t m_expectedSrcEpoch;

  // Number of the last finalized epoch
  uint32_t m_lastFinalizedEpoch;

  // Number of the last justified epoch
  uint32_t m_lastJustifiedEpoch;

  // Hash of the last checkpoint
  uint256 m_recommendedTargetHash;

  ufp64::ufp64_t m_lastVoterRescale;

  ufp64::ufp64_t m_lastNonVoterRescale;

  // Reward for voting as fraction of deposit size
  ufp64::ufp64_t m_rewardFactor;

  void InstaFinalize();

  void IncrementDynasty();

  ufp64::ufp64_t GetCollectiveRewardFactor();

  bool DepositExists();

  ufp64::ufp64_t GetSqrtOfTotalDeposits();

  uint32_t GetEpochsSinceFinalization();

  Result IsVotable(const Validator &validator, const uint256 &targetHash,
                   uint32_t targetEpoch, uint32_t sourceEpoch) const;

  bool IsInDynasty(const Validator &validator, uint32_t dynasty) const;

  CAmount ProcessReward(const uint256 &validatorIndex, uint64_t reward);

  void DeleteValidator(const uint256 &validatorIndex);

  uint64_t GetTotalCurDynDeposits();

  uint64_t GetTotalPrevDynDeposits();

  uint32_t GetEndDynasty() const;

  uint64_t CalculateVoteReward(const Validator &validator) const;

  // Finalization params
  const uint32_t EPOCH_LENGTH;
  const CAmount MIN_DEPOSIT_SIZE;
  const uint32_t DYNASTY_LOGOUT_DELAY;
  const uint32_t WITHDRAWAL_EPOCH_DELAY;
  const int64_t SLASH_FRACTION_MULTIPLIER;
  const int64_t BOUNTY_FRACTION_DENOMINATOR;
  const ufp64::ufp64_t BASE_INTEREST_FACTOR;
  const ufp64::ufp64_t BASE_PENALTY_FACTOR;
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_FINALIZATIONSTATE_H
