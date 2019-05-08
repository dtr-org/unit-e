// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_VOTE_H
#define UNITE_ESPERANZA_VOTE_H

#include <pubkey.h>
#include <serialize.h>
#include <stdint.h>
#include <uint256.h>

namespace esperanza {

class Vote {

 public:
  uint160 m_validator_address;

  uint256 m_target_hash;

  uint32_t m_source_epoch;

  uint32_t m_target_epoch;

  bool operator==(const Vote &rhs) const {
    return this->m_validator_address == rhs.m_validator_address &&
           this->m_target_hash == rhs.m_target_hash &&
           this->m_source_epoch == rhs.m_source_epoch &&
           this->m_target_epoch == rhs.m_target_epoch;
  }

  uint256 GetHash() const;
  std::string ToString() const;

  ADD_SERIALIZE_METHODS

  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_validator_address);
    READWRITE(m_target_hash);
    READWRITE(m_source_epoch);
    READWRITE(m_target_epoch);
  }
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_VOTE_H
