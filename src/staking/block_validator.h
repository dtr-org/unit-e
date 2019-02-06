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
#include <uint256.h>
#include <staking/validation_result.h>

#include <boost/optional.hpp>

#include <cstdint>

namespace staking {

//! \brief A component for validating blocks.
//!
//! This class is an interface.
class BlockValidator {

 public:
  //! \brief checks that the block has the right structure, but nothing else
  //!
  //! A well-formed block is supposed to follow the following structure:
  //! - at least one transaction (the coinbase transaction)
  //! - the coinbase transaction must be the first transaction
  //! - no other transaction maybe marked as coinbase transaction
  virtual BlockValidationResult CheckBlock(const CBlock &) const = 0;

  virtual ~BlockValidator() = default;

  static std::unique_ptr<BlockValidator> New(Dependency<blockchain::Behavior>);
};

}  // namespace staking

#endif  //UNIT_E_STAKE_VALIDATOR_H
