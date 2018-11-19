// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_PRIMITIVES_TXTYPE_H
#define UNITE_PRIMITIVES_TXTYPE_H

#include <better-enums/enum.h>
#include <stdint.h>

//! \brief The type of a transaction (see CTransaction)
//!
//! In Bitcoin transactions have versions and are always of the same type. In
//! UnitE transactions have a version and a type as transactions can be one of
//! different types. UnitE distinguishes different types of transactions for
//! implementing Proof-of-Stake.
// clang-format off
BETTER_ENUM(
  TxType,
  uint16_t,
  STANDARD = 0,
  COINSTAKE = 1,
  DEPOSIT = 2,
  VOTE = 3,
  LOGOUT = 4,
  SLASH = 5,
  WITHDRAW = 6,
  ADMIN = 7
)
// clang-format on

#endif  // UNITE_PRIMITIVES_TXTYPE_H
