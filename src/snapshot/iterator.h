// Copyright (c) 2018 The Unit-e developers
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
  explicit Iterator(std::unique_ptr<Indexer> indexer);
  ~Iterator();

  uint256 GetSnapshotHash() { return m_indexer->GetMeta().snapshot_hash; }
  uint256 GetBestBlockHash() { return m_indexer->GetMeta().block_hash; }
  uint256 GetStakeModifier() { return m_indexer->GetMeta().stake_modifier; }
  uint256 GetChainWork() { return m_indexer->GetMeta().chain_work; }
  uint64_t GetTotalUTXOSubsets() {
    return m_indexer->GetMeta().total_utxo_subsets;
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
  uint256 CalculateHash(const uint256 &stake_modifier,
                        const uint256 &chain_work);

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
