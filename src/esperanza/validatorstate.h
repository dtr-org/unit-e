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

    // UNIT-E TODO: replace IS_VALIDATING in favor of state->IsFinalizerVoting
    // as since BlockConnected is processed on a thread, this phase can be
    // set with the delay. Another issue is that AddToWalletIfInvolvingMe
    // is called before BlockConnected so the state is lagging one block behind
    IS_VALIDATING,

    WAITING_DEPOSIT_CONFIRMATION,
    WAITING_DEPOSIT_FINALIZATION
)
// clang-format on

class ValidatorState {
 public:
  using Phase = _Phase;

  Phase m_phase = Phase::NOT_VALIDATING;
  uint160 m_validator_address = uint160S("0");
  std::map<uint32_t, Vote> m_vote_map;

  uint32_t m_last_source_epoch = 0;
  uint32_t m_last_target_epoch = 0;

  inline bool HasDeposit() const {
    return !m_validator_address.IsNull();
  }

  ADD_SERIALIZE_METHODS

  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    int phase = static_cast<int>(+m_phase);
    READWRITE(phase);
    if (ser_action.ForRead()) {
      m_phase = Phase::_from_integral(phase);
    }
    READWRITE(m_validator_address);
    READWRITE(m_vote_map);
    READWRITE(m_last_source_epoch);
    READWRITE(m_last_target_epoch);
  }

  bool operator==(const ValidatorState &v) const {
    return m_phase == v.m_phase &&
           m_validator_address == v.m_validator_address &&
           m_vote_map == v.m_vote_map &&
           m_last_source_epoch == v.m_last_source_epoch &&
           m_last_target_epoch == v.m_last_target_epoch;
  }
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_VALIDATORSTATE_H
