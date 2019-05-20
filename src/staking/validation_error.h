// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#ifndef UNIT_E_STAKING_VALIDATION_ERROR_H
#define UNIT_E_STAKING_VALIDATION_ERROR_H

#include <better-enums/enum.h>

#include <cstdint>

class CValidationState;

namespace staking {

class BlockValidationResult;

// clang-format off
BETTER_ENUM(
    BlockValidationError,
    std::uint8_t,
    BLOCK_SIGNATURE_VERIFICATION_FAILED,
    BLOCKTIME_TOO_EARLY,
    BLOCKTIME_TOO_FAR_INTO_FUTURE,
    COINBASE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST,
    COINBASE_TRANSACTION_WITHOUT_OUTPUT,
    DUPLICATE_STAKE,
    DUPLICATE_TRANSACTION,
    FINALIZER_COMMITS_MERKLE_ROOT_MISMATCH,
    FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION,
    INVALID_BLOCK_HEIGHT,
    INVALID_BLOCK_TIME,
    INVALID_BLOCK_PUBLIC_KEY,
    INVALID_BLOCK_SIGOPS_COUNT,
    INVALID_BLOCK_WEIGHT,
    INVALID_COINBASE_LENGTH,
    INVALID_FINALIZER_COMMIT_BAD_SCRIPT,
    INVALID_TRANSACTION_DUPLICATE_INPUTS,
    INVALID_TRANSACTION_NEGATIVE_OUTPUT,
    INVALID_TRANSACTION_NO_INPUTS,
    INVALID_TRANSACTION_NO_OUTPUTS,
    INVALID_TRANSACTION_NULL_INPUT,
    INVALID_TRANSACTION_ORDERING,
    INVALID_TRANSACTION_OUTPUT_PAYS_TOO_MUCH,
    INVALID_TRANSACTION_PAYS_TOO_MUCH,
    INVALID_TRANSACTION_TOO_BIG,
    INVALID_META_INPUT_PREVOUT,
    MERKLE_ROOT_MISMATCH,
    MERKLE_ROOT_DUPLICATE_TRANSACTIONS,
    MISMATCHING_HEIGHT,
    NO_BLOCK_HEIGHT,
    NO_COINBASE_TRANSACTION,
    NO_META_INPUT,
    NO_SNAPSHOT_HASH,
    NO_STAKING_INPUT,
    NO_TRANSACTIONS,
    NON_FINAL_TRANSACTION,
    PREVIOUS_BLOCK_DOESNT_MATCH,
    PREVIOUS_BLOCK_NOT_PART_OF_ACTIVE_CHAIN,
    REMOTE_STAKING_INPUT_BIGGER_THAN_OUTPUT,
    STAKE_IMMATURE,
    STAKE_NOT_ELIGIBLE,
    STAKE_NOT_FOUND,
    TRANSACTION_INPUT_NOT_FOUND,
    WITNESS_MERKLE_ROOT_MISMATCH,
    WITNESS_MERKLE_ROOT_DUPLICATE_TRANSACTIONS
)
// clang-format on

const std::string &GetRejectionMessageFor(BlockValidationError);

bool CheckResult(const BlockValidationResult &result, CValidationState &state);

}  // namespace staking

#endif  //UNIT_E_VALIDATION_ERROR_H
