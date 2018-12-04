// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/validator.h>
#include <stdint.h>

namespace esperanza {

Validator::Validator()
    : m_validatorAddress(),
      m_deposit(0),
      m_startDynasty(0),
      m_endDynasty(DEFAULT_END_DYNASTY),
      m_isSlashed(false),
      m_depositsAtLogout(0) {}

Validator::Validator(uint64_t deposit, uint32_t startDynasty,
                     uint160 validatorAddress)
    : m_validatorAddress(validatorAddress),
      m_deposit(deposit),
      m_startDynasty(startDynasty),
      m_endDynasty(DEFAULT_END_DYNASTY),
      m_isSlashed(false),
      m_depositsAtLogout(0),
      m_lastTransactionHash(uint256()) {}

}  // namespace esperanza
