// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_VALIDATORSTATE_H
#define UNITE_ESPERANZA_VALIDATORSTATE_H

#include <better-enums/enum.h>
#include <esperanza/vote.h>
#include <primitives/transaction.h>
#include <stdint.h>
#include <sync.h>
#include <uint256.h>
#include <map>

namespace esperanza {

// clang-format off
BETTER_ENUM(
    _Phase,
    uint8_t,
    NOT_VALIDATING,
    IS_VALIDATING,
    WAITING_DEPOSIT_CONFIRMATION,
    WAITING_DEPOSIT_FINALIZATION
)
// clang-format on

struct ValidatorState {
  typedef _Phase Phase;

  ValidatorState()
      : m_validator_address(),
        m_last_esperanza_tx(nullptr),
        m_vote_map(),
        m_last_source_epoch(0),
        m_last_target_epoch(0),
        m_deposit_epoch(std::numeric_limits<uint32_t>::max()),
        m_end_dynasty(std::numeric_limits<uint32_t>::max()),
        m_start_dynasty(std::numeric_limits<uint32_t>::max()) {}

  Phase m_phase = Phase::NOT_VALIDATING;
  uint160 m_validator_address = uint160S("0");
  CTransactionRef m_last_esperanza_tx = nullptr;
  std::map<uint32_t, Vote> m_vote_map;

  uint32_t m_last_source_epoch;
  uint32_t m_last_target_epoch;
  uint32_t m_deposit_epoch;
  uint32_t m_end_dynasty;
  uint32_t m_start_dynasty;
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_VALIDATORSTATE_H
