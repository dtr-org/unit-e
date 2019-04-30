// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/validator.h>
#include <stdint.h>
#include <util.h>

namespace esperanza {

Validator::Validator()
    : m_validator_address(),
      m_deposit(0),
      m_start_dynasty(0),
      m_end_dynasty(MAX_END_DYNASTY),
      m_is_slashed(false),
      m_deposits_at_logout(0) {}

Validator::Validator(uint64_t deposit, uint32_t startDynasty,
                     uint160 validatorAddress)
    : m_validator_address(validatorAddress),
      m_deposit(deposit),
      m_start_dynasty(startDynasty),
      m_end_dynasty(MAX_END_DYNASTY),
      m_is_slashed(false),
      m_deposits_at_logout(0),
      m_last_transaction_hash(uint256()) {}

bool Validator::operator==(const Validator &other) const {
  return m_validator_address == other.m_validator_address &&
         m_deposit == other.m_deposit &&
         m_start_dynasty == other.m_start_dynasty &&
         m_end_dynasty == other.m_end_dynasty &&
         m_is_slashed == other.m_is_slashed &&
         m_deposits_at_logout == other.m_deposits_at_logout &&
         m_last_transaction_hash == other.m_last_transaction_hash;
}

std::string Validator::ToString() const {
  return strprintf(
      "Validator{"
      "m_validator_address=%s "
      "m_deposit=%d "
      "m_start_dynasty=%d "
      "m_end_dynasty=%d "
      "m_is_slashed=%d "
      "m_deposits_at_logout=%d "
      "m_last_transaction_hash=%s}",
      util::to_string(m_validator_address),
      m_deposit,
      m_start_dynasty,
      m_end_dynasty,
      m_is_slashed,
      m_deposits_at_logout,
      util::to_string(m_last_transaction_hash));
}

}  // namespace esperanza
