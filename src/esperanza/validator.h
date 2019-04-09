// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_VALIDATOR_H
#define UNITE_ESPERANZA_VALIDATOR_H

#include <serialize.h>
#include <uint256.h>

namespace esperanza {

static const uint32_t DEFAULT_END_DYNASTY =
    std::numeric_limits<uint32_t>::max();

class Validator {
 public:
  Validator();
  Validator(uint64_t deposit, uint32_t startDynasty, uint160 validatorAddress);
  bool operator==(const Validator &other) const;

  uint160 m_validator_address;
  uint64_t m_deposit;
  uint32_t m_start_dynasty;
  uint32_t m_end_dynasty;
  bool m_is_slashed;
  uint64_t m_deposits_at_logout;
  uint256 m_last_transaction_hash;

  ADD_SERIALIZE_METHODS

  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_validator_address);
    READWRITE(m_deposit);
    READWRITE(m_start_dynasty);
    READWRITE(m_end_dynasty);
    READWRITE(m_is_slashed);
    READWRITE(m_deposits_at_logout);
    READWRITE(m_last_transaction_hash);
  }

  std::string ToString() const;
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_VALIDATOR_H
