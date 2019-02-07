#ifndef UNIT_E_STAKING_VALIDATION_RESULT_H
#define UNIT_E_STAKING_VALIDATION_RESULT_H

#include <blockchain/blockchain_types.h>
#include <staking/validation_error.h>
#include <uint256.h>
#include <better-enums/enum_set.h>

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

}

#endif //UNIT_E_VALIDATION_RESULT_H
