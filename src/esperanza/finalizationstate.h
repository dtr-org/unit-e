// Copyright (c) 2018-2019 The Unit-e developers
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

class CChainParams;

namespace esperanza {

// clang-format off
BETTER_ENUM(
  Result,
  uint8_t,
  SUCCESS,
  INIT_WRONG_EPOCH,
  INIT_INVALID_REWARD,
  DEPOSIT_INSUFFICIENT,
  DEPOSIT_DUPLICATE,
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
 public:
  bool operator==(const FinalizationStateData &other) const;

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

  // Map of dynasty number to the starting epoch number
  std::map<uint32_t, uint32_t> m_dynastyStartEpoch;

  // List of validators
  std::map<uint160, Validator> m_validators;

  // Map of the dynasty number with the delta in deposits with the previous one
  std::map<uint32_t, uint64_t> m_dynastyDeltas;

  // Map of the epoch number with the deposit scale factor
  std::map<uint32_t, ufp64::ufp64_t> m_depositScaleFactor;

  // Map of the epoch number with the running total of deposits slashed
  std::map<uint32_t, uint64_t> m_totalSlashed;

  // The current epoch number
  uint32_t m_currentEpoch = 0;

  // The current dynasy number
  uint32_t m_currentDynasty = 0;

  // Total scaled deposits in the current dynasty
  uint64_t m_curDynDeposits = 0;

  // Total scaled deposits in the previous dynasty
  uint64_t m_prevDynDeposits = 0;

  // Expected epoch of the vote source
  uint32_t m_expectedSourceEpoch = 0;

  // Number of the last finalized epoch
  uint32_t m_lastFinalizedEpoch = 0;

  // Number of the last justified epoch
  uint32_t m_lastJustifiedEpoch = 0;

  // Last checkpoint
  const CBlockIndex *m_recommendedTarget = nullptr;

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
  //! \brief A status that represents the current stage in the finalization state initialization process.
  enum InitStatus {
    // State is just created
    NEW = 0,
    // State initialized from finalizer commits.
    FROM_COMMITS = 1,
    // State initialization completed using a block.
    COMPLETED = 2,
  };

  FinalizationState(const esperanza::FinalizationParams &params,
                    const esperanza::AdminParams &adminParams);
  FinalizationState(const FinalizationState &parent, InitStatus status = NEW);
  FinalizationState(FinalizationState &&parent);
  bool operator==(const FinalizationState &other) const;
  bool operator!=(const FinalizationState &other) const;

  //! \brief If the block height passed is the first of a new epoch, then we prepare the
  //! new epoch.
  //! @param blockHeight the block height.
  Result InitializeEpoch(blockchain::Height blockHeight);

  //! \brief Validates the consistency of the deposit against the current state.
  //!
  //! This does assume that the normal transaction validation process already took place.
  Result ValidateDeposit(const uint160 &validatorAddress,
                         CAmount depositValue) const;

  //! Performs a deposit for the given amount and for the validator with the given address.
  void ProcessDeposit(const uint160 &validatorAddress, CAmount depositValue);

  //! \brief Validates the vote of the deposit against the current state.
  //!
  //! This does assume that the normal transaction validation process already took place.
  Result ValidateVote(const Vote &vote) const;

  //! Performs a vote using the given vote input.
  void ProcessVote(const Vote &vote);

  //! \brief Validates the consistency of the logout against the current state.
  //!
  //! This does assume that the normal transaction validation process already took place.
  Result ValidateLogout(const uint160 &validatorAddress) const;

  //! Performs a logout for the validator with the given address.
  void ProcessLogout(const uint160 &validatorAddress);

  //! \brief Validates the consistency of the witdhraw against the current state.
  //!
  //! This does assume that the normal transaction validation process already took place.
  Result ValidateWithdraw(const uint160 &validatorAddress,
                          CAmount requestedWithdraw) const;

  Result CalculateWithdrawAmount(const uint160 &validatorAddress,
                                 CAmount &withdrawAmountOut) const;

  //! Performes a withdraw operation for the validator with the given address, in
  //! fact removing him from the validators map.
  void ProcessWithdraw(const uint160 &validatorAddress);

  //! \brief Checks whether two distinct votes from the same voter are proven being a
  //! slashable misbehaviour.
  Result IsSlashable(const Vote &vote1, const Vote &vote2) const;

  //! Given two votes, performs a slash against the validator who casted them.
  void ProcessSlash(const Vote &vote1, const Vote &vote2);

  bool IsPermissioningActive() const;
  Result ValidateAdminKeys(const AdminKeySet &adminKeys) const;
  void ProcessAdminCommands(const std::vector<AdminCommand> &commands);

  uint32_t GetLastJustifiedEpoch() const;
  uint32_t GetLastFinalizedEpoch() const;

  uint32_t GetCurrentEpoch() const;
  uint32_t GetCurrentDynasty() const;

  //! \brief Returns the height of checkpoint next to last finalized checkpoint. It must be justified.
  uint32_t GetCheckpointHeightAfterFinalizedEpoch() const;

  uint64_t GetDepositSize(const uint160 &validatorAddress) const;

  const CBlockIndex *GetRecommendedTarget() const;

  Vote GetRecommendedVote(const uint160 &validatorAddress) const;

  std::vector<Validator> GetActiveFinalizers() const;
  const Validator *GetValidator(const uint160 &validatorAddress) const;

  //! \brief Returns the finalization state for the given block.
  static FinalizationState *GetState(const CBlockIndex *block = nullptr);

  uint32_t GetEpochLength() const;
  uint32_t GetEpoch(const CBlockIndex &blockIndex) const;
  uint32_t GetEpoch(blockchain::Height blockHeight) const;

  //! \brief Returns the height of the first block of the epoch.
  blockchain::Height GetEpochStartHeight(uint32_t epoch) const;

  //! \brief Returns the height of the last block of the epoch.
  blockchain::Height GetEpochCheckpointHeight(uint32_t epoch) const;

  static bool ValidateDepositAmount(CAmount amount);

  //! \brief Processes the next chain (active or alternative) tip passed.
  //!
  //! This method encapsulates all the logic necessary to make the finalization
  //! state progress by one block.
  //! \param block_ndex
  //! \param block
  //! \return
  void ProcessNewTip(const CBlockIndex &block_index, const CBlock &block);
  void ProcessNewCommits(const CBlockIndex &block_index, const std::vector<CTransactionRef> &txes);

  //! \brief Retrives the hash of the last finalization transaction performed by the validator.
  uint256 GetLastTxHash(uint160 &validatorAddress) const;

  //! \brief Returns whether block on height blockHeight is checkpoint
  bool IsCheckpoint(blockchain::Height blockHeight) const;

  //! \brief Returns whether block on height blockHeight is justified checkpoint
  bool IsJustifiedCheckpoint(blockchain::Height blockHeight) const;

  //! \brief Returns whether block on height blockHeight is finalized checkpoint
  bool IsFinalizedCheckpoint(blockchain::Height blockHeight) const;

  //! \brief Returns the current initalization status
  InitStatus GetInitStatus() const;

 private:
  //!In case there is nobody available to justify we justify automatically.
  void InstaJustify();

  //! Increments the current dynasty if finalization has been reached.
  void IncrementDynasty();

  ufp64::ufp64_t GetCollectiveRewardFactor();

  //! Checks if ther is any deposit from validators in the curDyn and prevDyn.
  bool DepositExists() const;

  ufp64::ufp64_t GetSqrtOfTotalDeposits() const;
  uint32_t GetEpochsSinceFinalization() const;

  //! Checks if the validator can create a valid vote with the given parameters.
  Result IsVotable(const Validator &validator, const uint256 &targetHash,
                   uint32_t targetEpoch, uint32_t sourceEpoch) const;

  bool IsInDynasty(const Validator &validator, uint32_t dynasty) const;
  CAmount ProcessReward(const uint160 &validatorAddress, uint64_t reward);

  //! Removes a validator from the validator map.
  void DeleteValidator(const uint160 &validatorAddress);

  uint64_t GetTotalCurDynDeposits() const;
  uint64_t GetTotalPrevDynDeposits() const;
  uint32_t GetEndDynasty() const;
  uint64_t CalculateVoteReward(const Validator &validator) const;
  ufp64::ufp64_t GetDepositScaleFactor(uint32_t epoch) const;
  uint64_t GetTotalSlashed(uint32_t epoch) const;
  Checkpoint &GetCheckpoint(uint32_t epoch);
  const Checkpoint &GetCheckpoint(uint32_t epoch) const;
  uint64_t GetDynastyDelta(uint32_t dynasty);
  void RegisterLastTx(uint160 &validatorAddress, CTransactionRef tx);
  void ProcessNewCommit(const CTransactionRef &tx);

  mutable CCriticalSection cs_esperanza;

 protected:
  const FinalizationParams &m_settings;
  InitStatus m_status = NEW;

 private:
  void OnBlock(blockchain::Height blockHeight);
};

inline uint32_t GetEpoch(const CBlockIndex &blockIndex) { return FinalizationState::GetState()->GetEpoch(blockIndex); }
inline uint32_t GetEpoch(blockchain::Height blockHeight) { return FinalizationState::GetState()->GetEpoch(blockHeight); }

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_FINALIZATIONSTATE_H
