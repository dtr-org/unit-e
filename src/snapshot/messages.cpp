// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/messages.h>

#include <streams.h>
#include <version.h>

namespace snapshot {

namespace {
class Secp256k1Context {
 public:
  Secp256k1Context() {
    m_ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
  }
  ~Secp256k1Context() { secp256k1_context_destroy(m_ctx); }

  Secp256k1Context(const Secp256k1Context &) = delete;
  Secp256k1Context &operator=(const Secp256k1Context &) = delete;

  static secp256k1_context *Get() {
    static Secp256k1Context ctx;
    return ctx.m_ctx;
  }

 private:
  secp256k1_context *m_ctx;
};
}  // anonymous namespace

SnapshotHash::SnapshotHash() {
  secp256k1_multiset_init(Secp256k1Context::Get(), &m_multiset);
}

void SnapshotHash::AddUTXO(const UTXO &utxo) {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << utxo;

  secp256k1_multiset_add(Secp256k1Context::Get(), &m_multiset,
                         (const uint8_t *)stream.data(), stream.size());
}

void SnapshotHash::SubUTXO(const snapshot::UTXO &utxo) {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << utxo;

  secp256k1_multiset_remove(Secp256k1Context::Get(), &m_multiset,
                            (const uint8_t *)stream.data(), stream.size());
}

uint256 SnapshotHash::GetHash() {
  uint256 hash;
  secp256k1_multiset_finalize(Secp256k1Context::Get(), hash.begin(),
                              &m_multiset);
  return hash;
}

}  // namespace snapshot
