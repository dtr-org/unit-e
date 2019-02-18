#include <staking/validation_error.h>

#include <consensus/validation.h>
#include <staking/validation_result.h>

#include <cassert>
#include <string>

namespace {

struct ValidationError {
  std::string reject_reason;
  std::uint32_t level;
  std::uint32_t reject_code;
  bool corruption;

  explicit ValidationError(
      const char *const reject_reason,
      const std::uint32_t level = 100,
      const std::uint32_t reject_code = REJECT_INVALID,
      const bool corruption = false)
      : reject_reason(reject_reason),
        level(level),
        reject_code(reject_code),
        corruption(corruption) {}
};

const ValidationError &GetValidationErrorFor(const staking::BlockValidationError error) {
  switch (+error) {
    case staking::BlockValidationError::BLOCK_SIGNATURE_VERIFICATION_FAILED: {
      static ValidationError err("bad-blk-signature");
      return err;
    }
    case staking::BlockValidationError::BLOCKTIME_TOO_EARLY: {
      static ValidationError err("time-too-old");
      return err;
    }
    case staking::BlockValidationError::BLOCKTIME_TOO_FAR_INTO_FUTURE: {
      static ValidationError err("time-too-new");
      return err;
    }
    case staking::BlockValidationError::COINBASE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST: {
      static ValidationError err("bad-cb-out-of-order");
      return err;
    }
    case staking::BlockValidationError::COINBASE_TRANSACTION_WITHOUT_OUTPUT: {
      static ValidationError err("bad-cb-no-outputs");
      return err;
    }
    case staking::BlockValidationError::DUPLICATE_STAKE: {
      static ValidationError err("bad-stake-duplicate");
      return err;
    }
    case staking::BlockValidationError::DUPLICATE_TRANSACTIONS_IN_MERKLE_TREE: {
      static ValidationError err("bad-txns-duplicate");
      return err;
    }
    case staking::BlockValidationError::DUPLICATE_TRANSACTIONS_IN_WITNESS_MERKLE_TREE: {
      static ValidationError err("bad-txns-witness-duplicate");
      return err;
    }
    case staking::BlockValidationError::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION: {
      static ValidationError err("bad-cb-missing");
      return err;
    }
    case staking::BlockValidationError::INVALID_BLOCK_HEIGHT: {
      static ValidationError err("bad-cb-height");
      return err;
    }
    case staking::BlockValidationError::INVALID_BLOCK_TIME: {
      static ValidationError err("bad-blk-time");
      return err;
    }
    case staking::BlockValidationError::INVALID_BLOCK_PUBLIC_KEY: {
      static ValidationError err("bad-blk-public-key");
      return err;
    }
    case staking::BlockValidationError::MERKLE_ROOT_MISMATCH: {
      static ValidationError err("bad-txnmrklroot");
      return err;
    }
    case staking::BlockValidationError::MISMATCHING_HEIGHT: {
      static ValidationError err("bad-cb-height");
      return err;
    }
    case staking::BlockValidationError::NO_BLOCK_HEIGHT: {
      static ValidationError err("bad-cb-height-missing");
      return err;
    }
    case staking::BlockValidationError::NO_COINBASE_TRANSACTION: {
      static ValidationError err("bad-cb-missing");
      return err;
    }
    case staking::BlockValidationError::NO_META_INPUT: {
      static ValidationError err("bad-cb-meta-input-missing");
      return err;
    }
    case staking::BlockValidationError::NO_SNAPSHOT_HASH: {
      static ValidationError err("bad-cb-snapshot-hash-missing");
      return err;
    }
    case staking::BlockValidationError::NO_STAKING_INPUT: {
      static ValidationError err("bad-stake-missing");
      return err;
    }
    case staking::BlockValidationError::NO_TRANSACTIONS: {
      static ValidationError err("bad-blk-no-transactions");
      return err;
    }
    case staking::BlockValidationError::PREVIOUS_BLOCK_DOESNT_MATCH: {
      static ValidationError err("bad-blk-prev-block-mismatch");
      return err;
    }
    case staking::BlockValidationError::PREVIOUS_BLOCK_NOT_PART_OF_ACTIVE_CHAIN: {
      static ValidationError err("prev-blk-not-found", 10, 0);
      return err;
    }
    case staking::BlockValidationError::STAKE_IMMATURE: {
      static ValidationError err("bad-stake-immature");
      return err;
    }
    case staking::BlockValidationError::STAKE_NOT_ELIGIBLE: {
      static ValidationError err("bad-stake-not-eligible");
      return err;
    }
    case staking::BlockValidationError::STAKE_NOT_FOUND: {
      static ValidationError err("bad-stake-not-found");
      return err;
    }
    case staking::BlockValidationError::WITNESS_MERKLE_ROOT_MISMATCH: {
      static ValidationError err("bad-witness-merkle-match");
      return err;
    }
  }
  assert(false && "silence gcc warnings");
}

}  // namespace

namespace staking {

const std::string &GetRejectionMessageFor(const BlockValidationError error) {
  return GetValidationErrorFor(error).reject_reason;
}

bool CheckResult(const BlockValidationResult &result, CValidationState &state) {
  if (!result) {
    const ValidationError &validation_error = GetValidationErrorFor(*result.errors.begin());
    state.DoS(
        validation_error.level, false, validation_error.reject_code, validation_error.reject_reason,
        validation_error.corruption, result.errors.ToString());
    return false;
  }
  return true;
}

}  // namespace staking
