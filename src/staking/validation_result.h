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
  boost::optional<blockchain::Height> height = boost::none;
  boost::optional<uint256> snapshot_hash = boost::none;

  void operator+=(const BlockValidationResult &other);

  //! \brief Validation succeeded if there are no validation errors
  explicit operator bool() const;

  //! \brief Create a message suitable for usage in a REJECT p2p message.
  std::string GetRejectionMessage() const;
};

}

#endif //UNIT_E_VALIDATION_RESULT_H
