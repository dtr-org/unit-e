// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_PRIMITIVES_TXTYPE_H
#define UNITE_PRIMITIVES_TXTYPE_H

#include <better-enums/enum.h>
#include <stdint.h>

//! \brief The type of a transaction (see CTransaction)
//!
//! A description of the different unit-e transaction types is given in
//! ADR-18 (https://github.com/dtr-org/unit-e-docs/blob/master/adrs/2018-11-02-ADR-18%20unit-e%20Transaction%20Types.md)
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
