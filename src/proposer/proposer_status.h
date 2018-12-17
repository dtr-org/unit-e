// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_PROPOSER_STATUS_H
#define UNIT_E_PROPOSER_PROPOSER_STATUS_H

#include <better-enums/enum.h>

namespace proposer {

// clang-format off
BETTER_ENUM(
    Status,
    uint8_t,
    NOT_PROPOSING,
    IS_PROPOSING,
    NOT_PROPOSING_SYNCING_BLOCKCHAIN,
    NOT_PROPOSING_NO_PEERS,
    NOT_PROPOSING_NOT_ENOUGH_BALANCE,
    NOT_PROPOSING_WALLET_LOCKED
)
// clang-format on

}  // namespace proposer

#endif  // UNIT_E_PROPOSER_STATUS_H
