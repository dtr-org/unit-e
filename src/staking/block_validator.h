// Copyright (c) 2018 The Unit-e developers
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

#include <boost/optional.hpp>

#include <cstdint>

namespace staking {

// clang-format off
BETTER_ENUM(
    BlockValidationError,
    std::uint8_t,
    BLOCK_SIGNATURE_VERIFICATION_FAILED,
    COINBASE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST,
    COINBASE_TRANSACTION_WITHOUT_OUTPUT,
    DUPLICATE_TRANSACTIONS_IN_MERKLE_TREE,
    DUPLICATE_TRANSACTIONS_IN_WITNESS_MERKLE_TREE,
    FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION,
    INVALID_BLOCK_HEIGHT,
    INVALID_BLOCK_TIME,
    INVALID_BLOCK_PUBLIC_KEY,
    MERKLE_ROOT_MISMATCH,
    NO_BLOCK_HEIGHT,
    NO_META_INPUT,
    NO_SNAPSHOT_HASH,
    NO_STAKING_INPUT,
    NO_TRANSACTIONS,
    WITNESS_MERKLE_ROOT_MISMATCH
)
// clang-format on

class BlockValidationResult {
 public:
  EnumSet<BlockValidationError> errors;
  boost::optional<blockchain::Height> height = boost::none;
  boost::optional<uint256> snapshot_hash = boost::none;

  void operator+=(const BlockValidationResult &other);

  //! \brief Validation succeeded if there are no validation errors
  explicit operator bool() const;
};

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
