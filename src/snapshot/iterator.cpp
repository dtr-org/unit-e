// Copyright (c) 2018 The unit-e core developers
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

Iterator::Iterator(std::unique_ptr<Indexer> &&indexer)
    : m_indexer(std::move(indexer)),
      m_file(nullptr),
      m_readTotal(0),
      m_subsetLeft(0) {
  if (m_indexer->GetMeta().m_totalUTXOSubsets > 0) {
    Next();
  }
}

Iterator::~Iterator() { closeFile(); }

bool Iterator::Valid() {
  // there is an issue to read data
  if (!m_file) {
    return false;
  }

  return (m_readTotal <= m_indexer->GetMeta().m_totalUTXOSubsets);
}

void Iterator::Next() {
  if (m_readTotal > m_indexer->GetMeta().m_totalUTXOSubsets) {
    return;  // whole snapshot is read
  }

  if (m_readTotal == m_indexer->GetMeta().m_totalUTXOSubsets) {
    ++m_readTotal;  // mark as end of snapshot
    return;
  }

  // switch to the next file
  if (m_subsetLeft == 0) {
    closeFile();

    m_file = m_indexer->GetClosestIdx(m_readTotal, m_subsetLeft, m_readTotal);
    if (!m_file) {
      return;
    }
  }

  // CAutoFile is used as a helper to unserialize one utxoSubset record but we
  // don't want to close the file so we release the ownership right away
  CAutoFile f(m_file, SER_DISK, PROTOCOL_VERSION);
  f >> m_utxoSubset;
  ++m_readTotal;
  --m_subsetLeft;
  f.release();
}

bool Iterator::MoveCursorTo(uint64_t subsetIndex) {
  if (m_indexer->GetMeta().m_totalUTXOSubsets <= subsetIndex) {
    return false;
  }

  // prevent reading the first message twice
  // when after the initialization MoveCursorTo(0) is invoked
  if (m_readTotal == 1 && subsetIndex == 0) {
    return true;
  }

  closeFile();

  m_file = m_indexer->GetClosestIdx(subsetIndex, m_subsetLeft, m_readTotal);
  if (!m_file) {
    return false;
  }

  Next();  // consume the message

  // keep reading more messages
  // until the index is equal to requested
  while (m_readTotal < subsetIndex + 1) {
    if (!Valid()) {
      return false;
    }
    Next();
  }

  return true;
}

bool Iterator::GetUTXOSubsets(uint64_t subsetIndex, uint16_t count,
                              std::vector<UTXOSubset> &subsetsOut) {
  if (!MoveCursorTo(subsetIndex)) {
    return false;
  }

  // todo(kostia): don't return more than 4MB as it's the maximum allowed
  // message size in P2P. 10K UTXO Sets is ~1MB and on Bitcoin data doesn't grow
  // more than 1.2MB but theoretically it can go beyond the 4MB limit

  subsetsOut.clear();
  subsetsOut.reserve(count);

  uint16_t n = 0;
  while (Valid()) {
    if (n >= count) {
      break;
    }

    subsetsOut.emplace_back(GetUTXOSubset());
    ++n;

    Next();
  }

  return true;
}

uint256 Iterator::CalculateHash(uint256 stakeModifier) {
  // unwind to the beginning if needed
  if (m_readTotal > 1) {
    MoveCursorTo(0);
  }

  SnapshotHash hash;
  while (Valid()) {
    UTXOSubset subset = GetUTXOSubset();
    for (const auto &p : subset.m_outputs) {
      const COutPoint out(subset.m_txId, p.first);
      const Coin coin(p.second, subset.m_height, subset.m_isCoinBase);
      hash.AddUTXO(UTXO(out, coin));
    }

    Next();
  }

  return hash.GetHash(stakeModifier);
}

void Iterator::closeFile() {
  if (m_file) {
    fclose(m_file);
    m_file = nullptr;
  }
}

}  // namespace snapshot
