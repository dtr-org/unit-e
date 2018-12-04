// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_VOTE_H
#define UNITE_ESPERANZA_VOTE_H

#include <stdint.h>
#include <uint256.h>

class CKeyID;
class CKeyStore;

namespace esperanza {

class Vote {

 public:
  uint160 m_validatorAddress;

  uint256 m_targetHash;

  uint32_t m_sourceEpoch;

  uint32_t m_targetEpoch;

  bool operator==(const Vote &rhs) const {
    return this->m_validatorAddress == rhs.m_validatorAddress &&
           this->m_targetHash == rhs.m_targetHash &&
           this->m_sourceEpoch == rhs.m_sourceEpoch &&
           this->m_targetEpoch == rhs.m_targetEpoch;
  }

  static bool CreateSignature(CKeyStore *keystore, const Vote &vote,
                              std::vector<unsigned char> &voteSigOut);

  uint256 GetHash() const;

  std::string ToString() {
    return m_validatorIndex.GetHex() + ", " + m_targetHash.GetHex() + ", " +
           std::to_string(m_sourceEpoch) + ", " + std::to_string(m_targetEpoch);
  }
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_VOTE_H
