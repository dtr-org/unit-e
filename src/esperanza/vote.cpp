// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/vote.h>

namespace esperanza {

uint256 Vote::GetHash() const {

  CHashWriter ss(SER_GETHASH, 0);

  ss << m_validator_address;
  ss << m_target_hash;
  ss << m_source_epoch;
  ss << m_target_epoch;

  return ss.GetHash();
}

std::string Vote::ToString() const {
  return m_validator_address.GetHex() + ", " +
         m_target_hash.GetHex() + ", " +
         std::to_string(m_source_epoch) + ", " +
         std::to_string(m_target_epoch);
}
}  // namespace esperanza
