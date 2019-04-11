// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STAKING_STAKE_VALIDATOR_H
#define UNIT_E_STAKING_STAKE_VALIDATOR_H

#include <amount.h>
#include <blockchain/blockchain_behavior.h>
#include <chain.h>
#include <dependency.h>
#include <staking/active_chain.h>
#include <staking/block_validation_info.h>
#include <staking/coin.h>
#include <staking/validation_result.h>
#include <sync.h>
#include <uint256.h>
#include <validation_flags.h>

#include <memory>

namespace staking {

//! \brief
//!
//!
class StakeValidator {
 public:
  virtual CCriticalSection &GetLock() = 0;

  //! \brief Check whether a kernel and amount of stake meet the given difficulty.
  virtual bool CheckKernel(
      CAmount amount,                    //!< [in] The amount of stake which evens the odds
      const uint256 &kernel_hash,        //!< [in] The kernel hash to check
      blockchain::Difficulty difficulty  //!< [in] The target to check against
      ) const = 0;

  //! \brief Computes the kernel hash of a block.
  //!
  //! The kernel hash of a block is defined by a previous block, its stake
  //! (an output that the block references), and the time of this block.
  //!
  //! This function does not choose the block to be used as previous block.
  //! In PoS v3 it is just the preceding block, but it could be any previous
  //! block, for example the last finalized checkpoint.
  virtual uint256 ComputeKernelHash(
      const CBlockIndex *block_index,  //!< [in] The previous block to draw entropy from
      const staking::Coin &coin,       //!< [in] The stake to be used in this block
      blockchain::Time block_time      //!< [in] The time of this block
      ) const = 0;

  //! \brief Computes the stake modifier for a block.
  //!
  //! The stake modifier is not stored in a block on chain, but it is used
  //! to compute the kernel hash of a block that references this block as
  //! previous block.
  virtual uint256 ComputeStakeModifier(
      const CBlockIndex *block_index,  //!< [in] The previous block.
      const staking::Coin &coin        //!< [in] The staked coin.
      ) const = 0;

  //! \brief Checks the stake of a block and remote staking outputs in the coinbase transaction.
  //!
  //! Requires the lock for the active chain to be held
  //! (aka: cs_main, ActiveChain::GetLock()).
  //!
  //! Will lookup referenced block in the active chain, which means the block
  //! to be checked must be about to be connected as a new tip. The following
  //! data will be requested from the active chain:
  //!
  //! - the previous block to compute the stake modifier
  //! - the UTXOs which are spent in the coinbase transaction
  BlockValidationResult CheckStake(
      const CBlock &block,                                        //!< [in]
      BlockValidationInfo *info = nullptr,                        //!< [in,out]
      const CheckStakeFlags::Type flags = CheckStakeFlags::NONE,  //!< [in]
      const blockchain::UTXOView *utxo_view = nullptr             //!< [in]
      ) const {
    return CheckStake(block, utxo_view ? *utxo_view : GetUTXOView(), flags, info);
  }

  //! \brief Checks whether piece of stake was used as stake before.
  //!
  //! When a block refers to a piece of stake that another block that we've
  //! seen has refered to before, someone is trying to bullshit us and use a
  //! piece of stake twice.
  //!
  //! Requires the lock (obtained via GetLock) to be held.
  virtual bool IsPieceOfStakeKnown(
      const COutPoint &utxo  //!< [in] The reference to the UTXO used for staking.
      ) const = 0;

  //! \brief Learn about a piece of stake being used for staking.
  //!
  //! Requires the lock (obtained via GetLock) to be held.
  virtual void RememberPieceOfStake(
      const COutPoint &utxo  //!< [in] The reference to the UTXO used for staking.
      ) = 0;

  //! \brief Forget about a piece of stake having been used for staking.
  //!
  //! Requires the lock (obtained via GetLock) to be held.
  virtual void ForgetPieceOfStake(
      const COutPoint &utxo  //!< [in] The reference to the UTXO used for staking.
      ) = 0;

  virtual ~StakeValidator() = default;

  static std::unique_ptr<StakeValidator> New(
      Dependency<blockchain::Behavior>,
      Dependency<ActiveChain>);

 protected:
  virtual blockchain::UTXOView &GetUTXOView() const = 0;

  virtual BlockValidationResult CheckStake(
      const CBlock &block,                    //!< [in] The block to check.
      const blockchain::UTXOView &utxo_view,  //!< [in]
      CheckStakeFlags::Type flags,            //!< [in] options for checking stake, see CheckStakeFlags::Type
      BlockValidationInfo *info               //!< [in,out] Access to the validation info for this block (optional, nullptr may be passed).
      ) const = 0;
};

}  // namespace staking

#endif  //UNIT_E_STAKING_STAKE_VALIDATOR_H
