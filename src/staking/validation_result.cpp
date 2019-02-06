#include <staking/validation_result.h>

namespace staking {

void BlockValidationResult::operator+=(const BlockValidationResult &other) {
  errors += other.errors;
  if (other.height) {
    height = other.height;
  }
  if (other.snapshot_hash) {
    snapshot_hash = other.snapshot_hash;
  }
}

BlockValidationResult::operator bool() const {
  return errors.IsEmpty();
}

std::string BlockValidationResult::GetRejectionMessage() const {
  return errors.ToStringUsing(GetRejectionMessageFor);
}

}  // namespace staking
