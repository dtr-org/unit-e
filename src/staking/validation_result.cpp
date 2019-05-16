// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#include <staking/validation_result.h>

namespace staking {

const BlockValidationResult BlockValidationResult::success = BlockValidationResult();

BlockValidationResult::operator bool() const {
  return !m_error;
}

std::string BlockValidationResult::GetRejectionMessage() const {
  if (!m_error) {
    return "";
  }
  return GetRejectionMessageFor(*m_error);
}

bool BlockValidationResult::Is(const BlockValidationError error) const {
  if (!m_error) {
    return false;
  }
  return *m_error == +error;
}

BlockValidationError BlockValidationResult::operator*() const {
  return *m_error;
}

}  // namespace staking
