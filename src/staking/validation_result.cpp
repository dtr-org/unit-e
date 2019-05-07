// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#include <staking/validation_result.h>

namespace staking {

void BlockValidationResult::AddAll(const BlockValidationResult &other) {
  errors += other.errors;
}

void BlockValidationResult::AddError(const BlockValidationError error) {
  errors += error;
}

void BlockValidationResult::RemoveError(const BlockValidationError error) {
  errors -= error;
}

BlockValidationResult::operator bool() const {
  return errors.IsEmpty();
}

std::string BlockValidationResult::GetRejectionMessage() const {
  return errors.ToStringUsing(GetRejectionMessageFor);
}

}  // namespace staking
