// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_TYPES_H
#define UNIT_E_BLOCKCHAIN_TYPES_H

#include <amount.h>

#include <better-enums/enum.h>

#include <cstdint>

namespace blockchain {

// This header defines types that are used all across the project.

//! \brief difficulty, expressed in compact form ("nBits")
using Difficulty = std::uint32_t;
using Height = std::uint32_t;
using Depth = Height;
using Time = std::uint32_t;
using MoneySupply = CAmount;

// clang-format off
BETTER_ENUM(
    Network,
    uint8_t,
    main = 0,
    testnet = 1,
    regtest = 2
)
// clang-format on

}  // namespace blockchain

#endif  //UNIT_E_BLOCKCHAIN_TYPES_H
