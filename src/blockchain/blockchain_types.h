// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_TYPES_H
#define UNIT_E_BLOCKCHAIN_TYPES_H

#include <amount.h>

#include <better-enums/enum.h>
#include <uint256.h>

#include <cstdint>

namespace blockchain {

// This header defines types that are used all across the project.

//! \brief difficulty, expressed in compact form ("nBits")
using Difficulty = std::uint32_t;
using Height = std::uint32_t;
using Depth = Height;
using Time = std::uint32_t;

// clang-format off
BETTER_ENUM(
    Network,
    std::uint8_t,
    test = 1,
    regtest = 2
)
// clang-format on

// clang-format off
BETTER_ENUM(
    Base58Type,
    std::uint8_t,
    PUBKEY_ADDRESS,
    SCRIPT_ADDRESS,
    SECRET_KEY,
    EXT_PUBLIC_KEY,
    EXT_SECRET_KEY
)
// clang-format on

}  // namespace blockchain

#endif  //UNIT_E_BLOCKCHAIN_TYPES_H
