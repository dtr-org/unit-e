// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#ifndef UNIT_E_STAKING_VALIDATION_RESULT_H
#define UNIT_E_STAKING_VALIDATION_RESULT_H

#include <better-enums/enum_set.h>
#include <blockchain/blockchain_types.h>
#include <staking/validation_error.h>
#include <uint256.h>

#include <boost/optional.hpp>

namespace staking {

class BlockValidationResult {
 public:
  EnumSet<BlockValidationError> errors;

  //! \brief Add all errors from the given validation result to this one.
  void AddAll(const BlockValidationResult &other);

  //! \brief Add another error to this validation result.
  void AddError(BlockValidationError);

  //! \brief Remove an error from this validation result.
  void RemoveError(BlockValidationError);

  //! \brief Validation succeeded if there are no validation errors
  explicit operator bool() const;

  //! \brief Create a message suitable for usage in a REJECT p2p message.
  std::string GetRejectionMessage() const;
};

}  // namespace staking

#endif  //UNIT_E_VALIDATION_RESULT_H
