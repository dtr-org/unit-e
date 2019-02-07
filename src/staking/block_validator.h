// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STAKING_BLOCK_VALIDATOR_H
#define UNIT_E_STAKING_BLOCK_VALIDATOR_H

#include <better-enums/enum.h>
#include <better-enums/enum_set.h>
#include <blockchain/blockchain_behavior.h>
#include <dependency.h>
#include <primitives/block.h>
#include <staking/block_validation_info.h>
#include <staking/validation_result.h>
#include <uint256.h>

#include <boost/optional.hpp>

#include <cstdint>

namespace staking {

//! \brief A component for validating blocks and headers.
//!
//! This class is an interface.
//!
//! Design principles of the block validator:
//! - does not access the active chain or have any side effects.
//! - does not require any locks to be held.
//! - everything it needs to validate comes from the arguments passed to
//!   a function of from the currently active blockchain Behavior (which network).
//!
//! Since the previos call graph of validation functions was very hard to follow,
//! the relationship of the validation functions in the validator has been defined
//! in the following way:
//!
//! There are functions for validating:
//! (A) CBlockHeader
//! (B) CBlock
//!
//! And there are functions for validating:
//! (1) well-formedness (that is, values are in their proper place and look as they should)
//! (2) relation to the previous block
//!
//! A function of category (B) will always trigger the respective function from category (A)
//! first and continue only if that validated successfully.
//!
//! A function of category (2) will always trigger the respective function from category (1)
//! first and continue only if that validated successfully.
//!
//! All of these functions can be invoked passing a `BlockValidationInfo` pointer (which is
//! optional). If they are invoked with that they will track the state of validation and
//! don't do these checks again in case they have already been performed.
class BlockValidator {

 public:
  //! \brief checks that the block has the right structure, but nothing else.
  //!
  //! A well-formed block is supposed to follow the following structure:
  //! - at least one transaction (the coinbase transaction)
  //! - the coinbase transaction must be the first transaction
  //! - no other transaction maybe marked as coinbase transaction
  //!
  //! This function can be used to check the genesis block for well-formedness.
  //!
  //! The second parameter of this function is a reference to a BlockValidationInfo
  //! object which tracks validation information across invocations of different
  //! validation functions like CheckBlock, ContextualCheckBlock, CheckStake.
  //!
  //! Postconditions when invoked as `CheckBlock(block, blockValidationInfo)`:
  //! - (bool) BlockValidationResult == (bool) blockValidationInfo.GetCheckBlockStatus()
  //! - block.vtx.size() >= 1
  //! - block.vtx[0].GetType() == +TxType::COINBASE
  virtual BlockValidationResult CheckBlock(
      const CBlock &,        //!< [in] The block to check.
      BlockValidationInfo *  //!< [in,out] Access to the validation info for this block (optional, nullptr may be passed).
      ) const = 0;

  //! \brief checks the block with respect to its preceding block.
  //!
  //! This function can not be used to check the genesis block, as it does not have
  //! a preceding block.
  virtual BlockValidationResult ContextualCheckBlock(
      const CBlock &,        //!< [in] The block to check.
      const CBlockIndex &,   //!< [in] The block index entry of the preceding block.
      BlockValidationInfo *  //!< [in,out] Access to the validation info for this block (optional, nullptr may be passed).
      ) const = 0;

  //! \brief checks that the block header has the right structure, but nothing else.
  //!
  //! This function can be used to check the genssis block's header for well-formedness.
  virtual BlockValidationResult CheckBlockHeader(
      const CBlockHeader &,
      BlockValidationInfo *) const = 0;

  //! \brief checks the block header with resepect to its preceding block.
  //!
  //! This function can not be used to check the genesis block's eader, as that one
  //! does not have a preceding block.
  virtual BlockValidationResult ContextualCheckBlockHeader(
      const CBlockHeader &,
      const CBlockIndex &,
      BlockValidationInfo *) const = 0;

  virtual ~BlockValidator() = default;

  static std::unique_ptr<BlockValidator> New(Dependency<blockchain::Behavior>);
};

}  // namespace staking

#endif  //UNIT_E_STAKE_VALIDATOR_H
