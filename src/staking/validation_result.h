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
  static const BlockValidationResult success;

  //! \brief Construct a positive BlockValidationResult.
  BlockValidationResult() noexcept : m_error(boost::none) {}

  //! \brief Constructs a failed BlockValidationResult with an accompanying error code.
  explicit BlockValidationResult(const BlockValidationError error) : m_error(error) {}

  //! \brief Validation succeeded if there are no validation errors
  explicit operator bool() const;

  //! \brief Create a message suitable for usage in a REJECT p2p message.
  std::string GetRejectionMessage() const;

  bool Is(BlockValidationError error) const;

  BlockValidationError operator*() const;

 private:
  boost::optional<BlockValidationError> m_error;
};

}  // namespace staking

#endif  //UNIT_E_VALIDATION_RESULT_H
