// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_WALLETSTATE_H
#define UNITE_ESPERANZA_WALLETSTATE_H

#include <better-enums/enum.h>
#include <cstdint>

namespace esperanza {

// clang-format off
BETTER_ENUM(
    EncryptionState,
    uint8_t,
    UNENCRYPTED,
    LOCKED,
    UNLOCKED,
    UNLOCKED_FOR_STAKING_ONLY
);
// clang-format on

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_WALLETSTATE_H
