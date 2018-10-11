// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_ITERATOR_H
#define UNITE_SNAPSHOT_ITERATOR_H

#include <stdint.h>
#include <cstdio>
#include <memory>
#include <string>

#include <coins.h>
#include <primitives/transaction.h>
#include <snapshot/indexer.h>
#include <streams.h>
#include <uint256.h>

namespace snapshot {

// Iterator is not thread-safe
class Iterator {
 public:
  explicit Iterator(std::unique_ptr<Indexer> &&indexer);
  ~Iterator();

  uint256 GetSnapshotHash() { return m_indexer->GetMeta().m_snapshotHash; }
  uint32_t GetSnapshotId() { return m_indexer->GetSnapshotId(); }
  uint256 GetBestBlockHash() { return m_indexer->GetMeta().m_bestBlockHash; }
  uint64_t GetTotalTxUTXOSets() {
    return m_indexer->GetMeta().m_totalTxUTXOSets;
  }

  TxUTXOSet &GetTxUTXOSet() { return m_utxoSet; }
  bool GetTxUTXOSets(uint64_t utxoSetIndex, uint16_t count,
                     std::vector<TxUTXOSet> &utxoSetOut);

  bool Valid();
  void Next();
  bool MoveCursorTo(uint64_t utxoSetIndex);

 private:
  std::unique_ptr<Indexer> m_indexer;

  FILE *m_file;            // current opened file
  uint64_t m_readTotal;    // keep track of read all TxUTXOSet
  uint32_t m_utxoSetLeft;  // unread TxUTXOSet in the current file

  TxUTXOSet m_utxoSet;

  void closeFile();
};
}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_ITERATOR_H
