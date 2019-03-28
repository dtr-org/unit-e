// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/vote.h>
#include <key.h>
#include <keystore.h>

namespace esperanza {

bool Vote::CreateSignature(CKeyStore *keystore, const Vote &vote,
                           std::vector<unsigned char> &voteSigOut) {

  CKey privKey;
  if (!keystore->GetKey(CKeyID(vote.m_validator_address), privKey)) {
    return false;
  }

  return privKey.Sign(vote.GetHash(), voteSigOut);
}

bool Vote::CheckSignature(const CPubKey &pubkey, const Vote &vote,
                          std::vector<unsigned char> &voteSig) {
  return pubkey.Verify(vote.GetHash(), voteSig);
}

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
