// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#ifndef UNITE_PROPOSER_FINALIZATION_REWARD_LOGIC_H
#define UNITE_PROPOSER_FINALIZATION_REWARD_LOGIC_H

#include <blockchain/blockchain_behavior.h>
#include <blockdb.h>
#include <dependency.h>
#include <finalization/state_repository.h>

#include <memory>
#include <vector>

namespace proposer {

class MissingBlockError : public std::runtime_error {
 public:
  const CBlockIndex &missed_index;
  explicit MissingBlockError(const CBlockIndex &index) noexcept
      : std::runtime_error(strprintf("Cannot load block=%s", index.GetBlockHash().GetHex())),
        missed_index(index) {}
};

class FinalizationRewardLogic {
 public:
  //! \brief Calculate proposer finalization rewards
  //!
  //! The first block of every epoch (i.e. previous_block is a checkpoint) must
  //! contain finalization rewards for the block proposers of the previous
  //! epoch. The number of finalization reward outputs equals the epoch length.
  //! The reward size depends on the number of votes included in the previous
  //! epoch. The scripts are taken from the immediate reward outputs of the
  //! blocks of the previous epoch.
  //! If previous_block is not a checkpoint GetFinalizationRewards returns an
  //! empty vector.
  //!
  //! Note that the blocks of the previous epoch must be available on disk.
  virtual std::vector<CTxOut> GetFinalizationRewards(
      const CBlockIndex &previous_block  //!< [in] The previous block.
      ) const = 0;

  //! \brief Calculate proposer finalization reward amounts
  //!
  //! In contrast to GetFinalizationRewards, it does not retrieve the scripts
  //! and it can be used when the corresponding blocks are not stored on disk.
  virtual std::vector<CAmount> GetFinalizationRewardAmounts(
      const CBlockIndex &previous_block  //!< [in] The previous block.
      ) const = 0;

  //! \brief Get the number of finalization rewards for the block at a given height.
  //!
  //! The returned value is either the epoch length if the height corresponds
  //! to the start of an epoch or zero in other cases.
  virtual std::size_t GetNumberOfRewardOutputs(
      blockchain::Height height  //!< [in] The height of the block.
      ) const = 0;

  virtual ~FinalizationRewardLogic() = default;

  static std::unique_ptr<FinalizationRewardLogic> New(
      Dependency<blockchain::Behavior>,
      Dependency<finalization::Params> finalization_params,
      Dependency<finalization::StateRepository>,
      Dependency<BlockDB>);
};

}  // namespace proposer

#endif  // UNITE_PROPOSER_FINALIZATION_REWARD_LOGIC_H
