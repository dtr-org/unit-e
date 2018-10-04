// Copyright (c) 2018 The unit-e core developers
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
      : m_validatorIndex(),
        m_lastEsperanzaTx(nullptr),
        m_voteMap(),
        m_lastSourceEpoch(0),
        m_lastTargetEpoch(0),
        m_depositEpoch(std::numeric_limits<uint32_t>::max()),
        m_endDynasty(std::numeric_limits<uint32_t>::max()),
        m_startDynasty(std::numeric_limits<uint32_t>::max()) {}

  Phase m_phase = Phase::NOT_VALIDATING;
  uint256 m_validatorIndex;
  CTransactionRef m_lastEsperanzaTx;
  std::map<uint32_t, Vote> m_voteMap;

  uint32_t m_lastSourceEpoch;
  uint32_t m_lastTargetEpoch;
  uint32_t m_depositEpoch;
  uint32_t m_endDynasty;
  uint32_t m_startDynasty;
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_VALIDATORSTATE_H
