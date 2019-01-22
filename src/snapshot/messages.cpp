// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/messages.h>

#include <algorithm>

#include <chain.h>
#include <coins.h>
#include <streams.h>
#include <version.h>

namespace snapshot {

UTXO::UTXO(const COutPoint &out, const Coin &coin)
    : out_point(out),
      height(coin.nHeight),
      is_coin_base(coin.IsCoinBase()),
      tx_out(coin.out) {}

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

uint256 SnapshotHash::GetHash(const uint256 &stake_modifier,
                              const uint256 &chain_work) const {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << stake_modifier;
  stream << chain_work;

  // don't modify the existing hash with extra meta data
  secp256k1_multiset multiset = m_multiset;
  secp256k1_multiset_add(context, &multiset,
                         reinterpret_cast<const uint8_t *>(stream.data()),
                         stream.size());

  uint256 hash;
  secp256k1_multiset_finalize(context, hash.begin(), &multiset);
  return hash;
}

uint256 SnapshotHash::GetHash(const CBlockIndex &block_index) const {
  return GetHash(block_index.stake_modifier, ArithToUint256(block_index.nChainWork));
}

std::vector<uint8_t> SnapshotHash::GetHashVector(const CBlockIndex &block_index) const {
  uint256 hash = GetHash(block_index);
  return std::vector<uint8_t>(hash.begin(), hash.end());
}

void SnapshotHash::Clear() { secp256k1_multiset_init(context, &m_multiset); }

std::vector<uint8_t> SnapshotHash::GetData() const {
  std::vector<uint8_t> data(sizeof(m_multiset.d));
  std::copy(std::begin(m_multiset.d), std::end(m_multiset.d), data.begin());
  return data;
}

}  // namespace snapshot
