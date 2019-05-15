// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_FINALIZATIONSTATE_H
#define UNITE_ESPERANZA_FINALIZATIONSTATE_H

#include <better-enums/enum.h>
#include <esperanza/admincommand.h>
#include <esperanza/finalizationstate_data.h>
#include <finalization/params.h>

class CBlock;
class CBlockIndex;
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
  LOGOUT_NOT_YET_A_VALIDATOR,
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

  FinalizationState(const finalization::Params &params);
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
  Result ValidateVote(const Vote &vote, bool log_errors = true) const;

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
  Result IsSlashable(const Vote &vote1, const Vote &vote2, bool log_errors = true) const;

  //! Given two votes, performs a slash against the validator who casted them.
  void ProcessSlash(const Vote &vote1, const Vote &vote2);

  bool IsPermissioningActive() const;
  Result ValidateAdminKeys(const AdminKeySet &adminKeys) const;
  void ProcessAdminCommands(const std::vector<AdminCommand> &commands);

  uint32_t GetLastJustifiedEpoch() const;
  uint32_t GetLastFinalizedEpoch() const;

  uint32_t GetCurrentEpoch() const;
  uint32_t GetCurrentDynasty() const;

  uint64_t GetDepositSize(const uint160 &validatorAddress) const;

  uint32_t GetRecommendedTargetEpoch() const;

  Vote GetRecommendedVote(const uint160 &validatorAddress) const;

  std::vector<Validator> GetActiveFinalizers() const;
  const Validator *GetValidator(const uint160 &validatorAddress) const;

  uint32_t GetEpochLength() const;
  uint32_t GetEpoch(const CBlockIndex &blockIndex) const;
  uint32_t GetEpoch(blockchain::Height block_height) const;

  //! \brief Returns the height of the first block of the epoch.
  blockchain::Height GetEpochStartHeight(uint32_t epoch) const;

  //! \brief Returns the height of the last block of the epoch.
  blockchain::Height GetEpochCheckpointHeight(uint32_t epoch) const;

  bool ValidateDepositAmount(CAmount amount) const;

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
  uint256 GetLastTxHash(const uint160 &validatorAddress) const;

  //! \brief Returns whether block on height blockHeight is justified checkpoint
  bool IsJustifiedCheckpoint(blockchain::Height blockHeight) const;

  //! \brief Returns whether block on height blockHeight is finalized checkpoint
  bool IsFinalizedCheckpoint(blockchain::Height blockHeight) const;

  //! \brief Returns the current initalization status
  InitStatus GetInitStatus() const;

  //! \brief Returns true if finalizer can vote in current dynasty
  bool IsFinalizerVoting(const uint160 &finalizer_address) const;

  //! \brief Returns true if finalizer can vote in current dynasty
  bool IsFinalizerVoting(const Validator &finalizer) const;

  //! \brief Calculate the epoch at which finalizer can start withdrawing
  uint32_t CalculateWithdrawEpoch(const Validator &finalizer) const;

  //! Return the epoch when the current dynasty started
  uint32_t GetCurrentDynastyEpochStart() const;

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
                   uint32_t targetEpoch, uint32_t sourceEpoch,
                   bool log_errors) const;

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
  CAmount GetDynastyDelta(uint32_t dynasty);
  void RegisterLastTx(uint160 &validatorAddress, CTransactionRef tx);
  void ProcessNewCommit(const CTransactionRef &tx);

  mutable CCriticalSection cs_esperanza;

 protected:
  const finalization::Params &m_settings;
  InitStatus m_status = NEW;

 public:
  ADD_SERIALIZE_METHODS

  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(static_cast<FinalizationStateData &>(*this));
    int status = static_cast<int>(m_status);
    READWRITE(status);
    if (ser_action.ForRead()) {
      m_status = static_cast<InitStatus>(status);
    }
  }
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_FINALIZATIONSTATE_H
