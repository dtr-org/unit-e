// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/graphene_hasher.h>

namespace p2p {

GrapheneHasher::GrapheneHasher(const CBlockHeader &header,
                               uint64_t nonce) {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << header << nonce;
  CSHA256 hasher;
  hasher.Write((unsigned char *)&(*stream.begin()), stream.end() - stream.begin());
  uint256 shorttxidhash;
  hasher.Finalize(shorttxidhash.begin());
  m_k0 = shorttxidhash.GetUint64(0);
  m_k1 = shorttxidhash.GetUint64(1);
}

GrapheneShortHash GrapheneHasher::GetShortHash(const uint256 &full_hash) const {
  const auto short_hash = SipHashUint256(m_k0, m_k1, full_hash);

  static_assert(std::is_same<decltype(short_hash), const GrapheneShortHash>::value,
                "Type mismatch");

  return short_hash;
}

GrapheneFullHash GrapheneHasher::GetFullHash(const CTransaction &tx) const {
  return GrapheneFullHash(tx.GetWitnessHash());
}

GrapheneShortHash GrapheneHasher::GetShortHash(const CTransaction &tx) const {
  return GetShortHash(GetFullHash(tx));
}

}  // namespace p2p
