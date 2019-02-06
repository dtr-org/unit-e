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
#include <staking/coin.h>
#include <staking/validation_result.h>
#include <sync.h>
#include <uint256.h>

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
      CAmount,                //!< [in] The amount of stake which evens the odds
      const uint256 &,        //!< [in] The kernel hash to check
      blockchain::Difficulty  //!< [in] The target to check against
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
      const CBlockIndex *,    //!< [in] The previous block to draw entropy from
      const staking::Coin &,  //!< [in] The stake to be used in this block
      blockchain::Time        //!< [in] The time of this block
      ) const = 0;

  //! \brief Computes the stake modifier for a block.
  //!
  //! The stake modifier is not stored in a block on chain, but it is used
  //! to compute the kernel hash of a block that references this block as
  //! previous block.
  virtual uint256 ComputeStakeModifier(
      const CBlockIndex *,  //!< [in] The previous block
      const uint256 &       //!< [in] The kernel hash of this block
      ) const = 0;

  //! \brief Checks the stake of a block.
  virtual BlockValidationResult CheckStake(
      const CBlock &  //!< [in] The block to check.
      ) const = 0;

  //! \brief Checks whether piece of stake was used as stake before.
  //!
  //! When a block refers to a piece of stake that another block that we've
  //! seen has refered to before, someone is trying to bullshit us and use a
  //! piece of stake twice.
  //!
  //! Requires the lock (obtained via GetLock) to be held.
  virtual bool IsPieceOfStakeKnown(
      const COutPoint &  //!< [in] The reference to the UTXO used for staking.
      ) const = 0;

  //! \brief Learn about a piece of stake being used for staking.
  //!
  //! Requires the lock (obtained via GetLock) to be held.
  virtual void RememberPieceOfStake(
      const COutPoint &  //!< [in] The reference to the UTXO used for staking.
      ) = 0;

  //! \brief Forget about a piece of stake having been used for staking.
  //!
  //! Requires the lock (obtained via GetLock) to be held.
  virtual void ForgetPieceOfStake(
      const COutPoint &  //!< [in] The reference to the UTXO used for staking.
      ) = 0;

  virtual ~StakeValidator() = default;

  static std::unique_ptr<StakeValidator> New(
      Dependency<blockchain::Behavior>,
      Dependency<ActiveChain>);
};

}  // namespace staking

#endif  //UNIT_E_STAKING_STAKE_VALIDATOR_H
