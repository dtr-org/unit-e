// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/messages.h>

#include <streams.h>
#include <version.h>

namespace snapshot {

secp256k1_context *context = nullptr;

bool CreateSecp256k1Context() {
  context = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
  return context != nullptr;
}

void DeleteSecp256k1Context() { secp256k1_context_destroy(context); }

SnapshotHash::SnapshotHash() { secp256k1_multiset_init(context, &m_multiset); }

void SnapshotHash::AddUTXO(const UTXO &utxo) {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << utxo;

  secp256k1_multiset_add(context, &m_multiset,
                         reinterpret_cast<const uint8_t *>(stream.data()),
                         stream.size());
}

void SnapshotHash::SubUTXO(const snapshot::UTXO &utxo) {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << utxo;

  secp256k1_multiset_remove(context, &m_multiset,
                            reinterpret_cast<const uint8_t *>(stream.data()),
                            stream.size());
}

uint256 SnapshotHash::GetHash() {
  uint256 hash;
  secp256k1_multiset_finalize(context, hash.begin(), &m_multiset);
  return hash;
}

}  // namespace snapshot
