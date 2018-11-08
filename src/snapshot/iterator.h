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
  uint64_t GetTotalUTXOSubsets() {
    return m_indexer->GetMeta().m_totalUTXOSubsets;
  }

  UTXOSubset &GetUTXOSubset() { return m_utxoSubset; }
  bool GetUTXOSubsets(uint64_t subsetIndex, uint16_t count,
                      std::vector<UTXOSubset> &subsetsOut);

  bool Valid();
  void Next();
  bool MoveCursorTo(uint64_t subsetIndex);

  //! CalculateHash calculates the hash of the full snapshot content.
  //! After calling this function the cursor will be invalid. To re-use the
  //! iterator again, it must be explicitly unwind to the beginning.
  //! iter->MoveCursorTo(0)
  uint256 CalculateHash(uint256 stakeModifier);

 private:
  std::unique_ptr<Indexer> m_indexer;

  FILE *m_file;           // current opened file
  uint64_t m_readTotal;   // keep track of read all UTXOSubset
  uint32_t m_subsetLeft;  // unread UTXOSubset in the current file

  UTXOSubset m_utxoSubset;

  void closeFile();
};
}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_ITERATOR_H
