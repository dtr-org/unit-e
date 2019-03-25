// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_VALIDATORSTATE_H
#define UNITE_ESPERANZA_VALIDATORSTATE_H

#include <better-enums/enum.h>
#include <esperanza/vote.h>
#include <primitives/transaction.h>
#include <serialize.h>
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

  Phase m_phase = Phase::NOT_VALIDATING;
  uint160 m_validator_address = uint160S("0");
  CTransactionRef m_last_esperanza_tx = nullptr;
  std::map<uint32_t, Vote> m_vote_map;

  uint32_t m_last_source_epoch = 0;
  uint32_t m_last_target_epoch = 0;
  uint32_t m_deposit_epoch = std::numeric_limits<uint32_t>::max();
  uint32_t m_end_dynasty = std::numeric_limits<uint32_t>::max();
  uint32_t m_start_dynasty = std::numeric_limits<uint32_t>::max();

  ADD_SERIALIZE_METHODS

  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    int phase = static_cast<int>(+m_phase);
    READWRITE(phase);
    if (ser_action.ForRead()) {
      m_phase = Phase::_from_integral(phase);
    }
    READWRITE(m_validator_address);
    bool has_tx = m_last_esperanza_tx != nullptr;
    READWRITE(has_tx);
    if (has_tx) {
      READWRITE(m_last_esperanza_tx);
    }
    READWRITE(m_vote_map);
    READWRITE(m_last_source_epoch);
    READWRITE(m_last_target_epoch);
    READWRITE(m_deposit_epoch);
    READWRITE(m_end_dynasty);
    READWRITE(m_start_dynasty);
  }

  bool operator==(const ValidatorState &v) const {
    return m_phase == v.m_phase &&
           m_validator_address == v.m_validator_address &&
           ((m_last_esperanza_tx != nullptr && v.m_last_esperanza_tx != nullptr &&
             m_last_esperanza_tx->GetHash() == v.m_last_esperanza_tx->GetHash()) ||
            (m_last_esperanza_tx == nullptr && v.m_last_esperanza_tx == nullptr)) &&
           m_vote_map == v.m_vote_map &&
           m_last_source_epoch == v.m_last_source_epoch &&
           m_last_target_epoch == v.m_last_target_epoch &&
           m_deposit_epoch == v.m_deposit_epoch &&
           m_end_dynasty == v.m_end_dynasty &&
           m_start_dynasty == v.m_start_dynasty;
  }
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_VALIDATORSTATE_H
