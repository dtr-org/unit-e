// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/messages.h>

#include <algorithm>

#include <coins.h>
#include <streams.h>
#include <version.h>

namespace snapshot {

UTXO::UTXO(const COutPoint &out, const Coin &coin)
    : m_outPoint(out),
      m_height(coin.nHeight),
      m_isCoinBase(coin.IsCoinStake()),
      m_txOut(coin.out) {}

secp256k1_context *context = nullptr;

bool InitSecp256k1Context() {
  context = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
  return context != nullptr;
}

void DestroySecp256k1Context() { secp256k1_context_destroy(context); }

SnapshotHash::SnapshotHash() { Clear(); }

SnapshotHash::SnapshotHash(const std::vector<uint8_t> &data) {
  assert(data.size() == sizeof(m_multiset.d));
  std::copy(data.begin(), data.end(), m_multiset.d);
}

void SnapshotHash::AddUTXO(const UTXO &utxo) {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << utxo;

  secp256k1_multiset_add(context, &m_multiset,
                         reinterpret_cast<const uint8_t *>(stream.data()),
                         stream.size());
}

void SnapshotHash::SubtractUTXO(const UTXO &utxo) {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << utxo;

  secp256k1_multiset_remove(context, &m_multiset,
                            reinterpret_cast<const uint8_t *>(stream.data()),
                            stream.size());
}

uint256 SnapshotHash::GetHash(uint256 stakeModifier) const {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << stakeModifier;

  // don't modify the existing hash with extra meta data
  secp256k1_multiset multiset = m_multiset;
  secp256k1_multiset_add(context, &multiset,
                         reinterpret_cast<const uint8_t *>(stream.data()),
                         stream.size());

  uint256 hash;
  secp256k1_multiset_finalize(context, hash.begin(), &multiset);
  return hash;
}

std::vector<uint8_t> SnapshotHash::GetHashVector(uint256 stakeModifier) const {
  uint256 hash = GetHash(stakeModifier);
  return std::vector<uint8_t>(hash.begin(), hash.end());
}

void SnapshotHash::Clear() { secp256k1_multiset_init(context, &m_multiset); }

std::vector<uint8_t> SnapshotHash::GetData() const {
  std::vector<uint8_t> data(sizeof(m_multiset.d));
  std::copy(std::begin(m_multiset.d), std::end(m_multiset.d), data.begin());
  return data;
}

}  // namespace snapshot
