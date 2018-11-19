// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_VALIDATOR_H
#define UNITE_ESPERANZA_VALIDATOR_H

#include <amount.h>
#include <pubkey.h>

namespace esperanza {

static const uint32_t DEFAULT_END_DYNASTY =
    std::numeric_limits<uint32_t>::max();

class Validator {
 public:
  Validator();
  Validator(uint64_t deposit, uint32_t startDynasty, uint160 validatorAddress);

  uint160 m_validatorAddress;
  uint64_t m_deposit;
  uint32_t m_startDynasty;
  uint32_t m_endDynasty;
  bool m_isSlashed;
  uint64_t m_depositsAtLogout;
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_VALIDATOR_H
