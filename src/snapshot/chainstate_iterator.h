// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_CHAINSTATE_ITERATOR_H
#define UNITE_CHAINSTATE_ITERATOR_H

#include <stdint.h>
#include <map>
#include <memory>

#include <primitives/transaction.h>
#include <snapshot/messages.h>
#include <txdb.h>
#include <uint256.h>

namespace snapshot {

class ChainstateIterator {
 public:
  explicit ChainstateIterator(CCoinsViewDB *view);
  bool Valid();
  void Next();
  const UTXOSubset &GetUTXOSubset() const { return m_utxoSubset; }
  const uint256 &GetBestBlock() const { return m_cursor->GetBestBlock(); }
  const SnapshotHash &GetSnapshotHash() const {
    return m_cursor->GetSnapshotHash();
  }

 private:
  bool m_valid;
  std::unique_ptr<CCoinsViewCursor> m_cursor;
  std::map<uint32_t, CTxOut> m_outputs;
  Coin m_prevCoin;
  uint256 m_prevTxId;
  UTXOSubset m_utxoSubset;
};

}  // namespace snapshot

#endif  // UNITE_CHAINSTATE_ITERATOR_H
