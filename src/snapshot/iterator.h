// Copyright (c) 2018-2019 The Unit-e developers
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
#include <snapshot/messages.h>
#include <streams.h>
#include <uint256.h>

namespace snapshot {

// Iterator is not thread-safe
class Iterator {
 public:
  explicit Iterator(std::unique_ptr<Indexer> indexer);
  ~Iterator();

  const SnapshotHeader &GetSnapshotHeader() { return m_indexer->GetSnapshotHeader(); }
  UTXOSubset &GetUTXOSubset() { return m_utxo_subset; }
  bool GetUTXOSubsets(uint64_t subset_index, uint16_t count,
                      std::vector<UTXOSubset> &subsets_out);

  bool Valid();
  void Next();
  bool MoveCursorTo(uint64_t subset_index);

  //! CalculateHash calculates the hash of the full snapshot content.
  //! After calling this function the cursor will be invalid. To re-use the
  //! iterator again, it must be explicitly unwind to the beginning.
  //! iter->MoveCursorTo(0)
  uint256 CalculateHash(const uint256 &stake_modifier,
                        const uint256 &chain_work);

 private:
  std::unique_ptr<Indexer> m_indexer;

  FILE *m_file;            // current opened file
  uint64_t m_read_total;   // keep track of read all UTXOSubset
  uint32_t m_subset_left;  // unread UTXOSubset in the current file

  UTXOSubset m_utxo_subset;

  void CloseFile();
};
}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_ITERATOR_H
