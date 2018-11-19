// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STATUS_H
#define UNIT_E_STATUS_H

#include <better-enums/enum.h>

// clang-format off
BETTER_ENUM(
  SyncStatus,
  uint8_t,
  SYNCED,
  IMPORTING,
  REINDEXING,
  NO_TIP,
  MINIMUM_CHAIN_WORK_NOT_REACHED,
  MAX_TIP_AGE_EXCEEDED
)
// clang-format on

#endif  // UNIT_E_STATUS_H
