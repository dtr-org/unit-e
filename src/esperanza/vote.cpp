#include <esperanza/vote.h>
#include <key.h>
#include <keystore.h>

namespace esperanza {

bool Vote::CreateSignature(CKeyStore *keystore, const Vote &vote,
                           std::vector<unsigned char> &voteSigOut) {

  CKeyID address = CKeyID(uint160(
      Hash160(vote.m_validatorIndex.begin(), vote.m_validatorIndex.end())));

  CKey privKey;
  if (!keystore->GetKey(address, privKey))
    return false;

  return privKey.Sign(vote.GetHash(), voteSigOut);
}

uint256 Vote::GetHash() const {

  CHashWriter ss(SER_GETHASH, 0);

  ss << m_validatorIndex;
  ss << m_sourceEpoch;
  ss << m_targetEpoch;
  ss << m_targetHash;

  return ss.GetHash();
}

}  // namespace esperanza
