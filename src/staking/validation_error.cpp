#include <staking/validation_error.h>

#include <string>
#include <cassert>

namespace staking {

std::string GetRejectionMessageFor(const BlockValidationError error) {
  switch (+error) {
    case BlockValidationError::BLOCK_SIGNATURE_VERIFICATION_FAILED:
      return "bad-blk-signature";
    case BlockValidationError::BLOCKTIME_TOO_EARLY:
      return "time-too-old";
    case BlockValidationError::BLOCKTIME_TOO_FAR_INTO_FUTURE:
      return "time-too-new";
    case BlockValidationError::COINBASE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST:
      return "bad-cp-out-of-order";
    case BlockValidationError::COINBASE_TRANSACTION_WITHOUT_OUTPUT:
      return "bad-cp-no-outputs";
    case BlockValidationError::DUPLICATE_STAKE:
      return "bad-stake-duplicate";
    case BlockValidationError::DUPLICATE_TRANSACTIONS_IN_MERKLE_TREE:
      return "bad-txns-duplicate";
    case BlockValidationError::DUPLICATE_TRANSACTIONS_IN_WITNESS_MERKLE_TREE:
      return "bad-txns-witness-duplicate";
    case BlockValidationError::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION:
      return "bad-cb-missing";
    case BlockValidationError::INVALID_BLOCK_HEIGHT:
      return "bad-cb-height";
    case BlockValidationError::INVALID_BLOCK_TIME:
      return "bad-blk-time";
    case BlockValidationError::INVALID_BLOCK_PUBLIC_KEY:
      return "bad-blk-public-key";
    case BlockValidationError::MERKLE_ROOT_MISMATCH:
      return "bad-txnmrklroot";
    case BlockValidationError::NO_BLOCK_HEIGHT:
      return "bad-cb-height-missing";
    case BlockValidationError::NO_COINBASE_TRANSACTION:
      return "bad-cb-missing";
    case BlockValidationError::NO_META_INPUT:
      return "bad-cb-meta-input-missing";
    case BlockValidationError::NO_SNAPSHOT_HASH:
      return "bad-cb-snapshot-hash-missing";
    case BlockValidationError::NO_STAKING_INPUT:
      return "bad-stake-missing";
    case BlockValidationError::NO_TRANSACTIONS:
      return "bad-blk-no-transactions";
    case BlockValidationError::PREVIOUS_BLOCK_DOESNT_MATCH:
      return "bad-blk-prev-block-mismatch";
    case BlockValidationError::PREVIOUS_BLOCK_NOT_PART_OF_ACTIVE_CHAIN:
      return "bad-blk-prev-block-missing";
    case BlockValidationError::STAKE_IMMATURE:
      return "bad-stake-immature";
    case BlockValidationError::STAKE_NOT_ELIGIBLE:
      return "bad-stake-not-eligible";
    case BlockValidationError::STAKE_NOT_FOUND:
      return "bad-stake-not-found";
    case BlockValidationError::WITNESS_MERKLE_ROOT_MISMATCH:
      return "bad-witness-merkle-match";
  }
  assert(false && "silence gcc warnings");
}

}  // namespace staking
