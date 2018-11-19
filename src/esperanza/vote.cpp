// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "vote.h"
#include <esperanza/vote.h>
#include <key.h>
#include <keystore.h>

namespace esperanza {

bool Vote::CreateSignature(CKeyStore *keystore, const Vote &vote,
                           std::vector<unsigned char> &voteSigOut) {

  CKey privKey;
  if (!keystore->GetKey(CKeyID(vote.m_validatorAddress), privKey)) {
    return false;
  }

  return privKey.Sign(vote.GetHash(), voteSigOut);
}

uint256 Vote::GetHash() const {

  CHashWriter ss(SER_GETHASH, 0);

  ss << m_validatorAddress;
  ss << m_targetHash;
  ss << m_sourceEpoch;
  ss << m_targetEpoch;

  return ss.GetHash();
}

std::string Vote::ToString() const {
  return m_validatorAddress.GetHex() + ", " + m_targetHash.GetHex() + ", " +
         std::to_string(m_sourceEpoch) + ", " + std::to_string(m_targetEpoch);
}
}  // namespace esperanza
