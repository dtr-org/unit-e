// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/iterator.h>

#include <map>
#include <stdexcept>

#include <clientversion.h>
#include <fs.h>
#include <serialize.h>
#include <util.h>
#include <version.h>

namespace snapshot {

Iterator::Iterator(std::unique_ptr<Indexer> indexer)
    : m_indexer(std::move(indexer)),
      m_file(nullptr),
      m_read_total(0),
      m_subset_left(0) {
  if (m_indexer->GetSnapshotHeader().total_utxo_subsets > 0) {
    Next();
  }
}

Iterator::~Iterator() { CloseFile(); }

bool Iterator::Valid() {
  // there is an issue to read data
  if (!m_file) {
    return false;
  }

  return (m_read_total <= m_indexer->GetSnapshotHeader().total_utxo_subsets);
}

void Iterator::Next() {
  if (m_read_total > m_indexer->GetSnapshotHeader().total_utxo_subsets) {
    return;  // whole snapshot is read
  }

  if (m_read_total == m_indexer->GetSnapshotHeader().total_utxo_subsets) {
    ++m_read_total;  // mark as end of snapshot
    return;
  }

  // switch to the next file
  if (m_subset_left == 0) {
    CloseFile();

    m_file = m_indexer->GetClosestIdx(m_read_total, m_subset_left, m_read_total);
    if (!m_file) {
      return;
    }
  }

  // CAutoFile is used as a helper to unserialize one utxoSubset record but we
  // don't want to close the file so we release the ownership right away
  CAutoFile f(m_file, SER_DISK, PROTOCOL_VERSION);
  f >> m_utxo_subset;
  ++m_read_total;
  --m_subset_left;
  f.release();
}

bool Iterator::MoveCursorTo(const uint64_t subset_index) {
  if (m_indexer->GetSnapshotHeader().total_utxo_subsets <= subset_index) {
    return false;
  }

  // prevent reading the first message twice
  // when after the initialization MoveCursorTo(0) is invoked
  if (m_read_total == 1 && subset_index == 0) {
    return true;
  }

  CloseFile();

  m_file = m_indexer->GetClosestIdx(subset_index, m_subset_left, m_read_total);
  if (!m_file) {
    return false;
  }

  Next();  // consume the message

  // keep reading more messages
  // until the index is equal to requested
  while (m_read_total < subset_index + 1) {
    if (!Valid()) {
      return false;
    }
    Next();
  }

  return true;
}

bool Iterator::GetUTXOSubsets(const uint64_t subset_index, const uint16_t count,
                              std::vector<UTXOSubset> &subsets_out) {
  if (!MoveCursorTo(subset_index)) {
    return false;
  }

  // todo(kostia): don't return more than 4MB as it's the maximum allowed
  // message size in P2P. 10K UTXO Sets is ~1MB and on Bitcoin data doesn't grow
  // more than 1.2MB but theoretically it can go beyond the 4MB limit

  subsets_out.clear();
  subsets_out.reserve(count);

  uint16_t n = 0;
  while (Valid()) {
    if (n >= count) {
      break;
    }

    subsets_out.emplace_back(GetUTXOSubset());
    ++n;

    Next();
  }

  return true;
}

uint256 Iterator::CalculateHash(const uint256 &stake_modifier,
                                const uint256 &chain_work) {
  // unwind to the beginning if needed
  if (m_read_total > 1) {
    MoveCursorTo(0);
  }

  SnapshotHash hash;
  while (Valid()) {
    UTXOSubset subset = GetUTXOSubset();
    for (const auto &p : subset.outputs) {
      const COutPoint out(subset.tx_id, p.first);
      const Coin coin(p.second, subset.height, subset.is_coin_base);
      hash.AddUTXO(UTXO(out, coin));
    }

    Next();
  }

  return hash.GetHash(stake_modifier, chain_work);
}

void Iterator::CloseFile() {
  if (m_file) {
    fclose(m_file);
    m_file = nullptr;
  }
}

}  // namespace snapshot
