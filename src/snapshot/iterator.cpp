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

Iterator::Iterator(std::shared_ptr<Indexer> indexer)
    : m_indexer(std::move(indexer)),
      m_file(nullptr),
      m_readTotal(0),
      m_utxoSetLeft(0) {
  if (m_indexer->GetMeta().m_totalUTXOSets > 0) {
    Next();
  }
}

Iterator::~Iterator() { closeFile(); }

bool Iterator::Valid() {
  // there is an issue to read data
  if (!m_file) {
    return false;
  }

  return (m_readTotal <= m_indexer->GetMeta().m_totalUTXOSets);
}

void Iterator::Next() {
  if (m_readTotal > m_indexer->GetMeta().m_totalUTXOSets) {
    return;  // whole snapshot is read
  }

  if (m_readTotal == m_indexer->GetMeta().m_totalUTXOSets) {
    ++m_readTotal;  // mark as end of snapshot
    return;
  }

  // switch to the next file
  if (m_utxoSetLeft == 0) {
    closeFile();

    m_file = m_indexer->GetClosestIdx(m_readTotal, m_utxoSetLeft, m_readTotal);
    if (!m_file) {
      return;
    }
  }

  // CAutoFile is used as a helper to unserialize one utxoSet record but we
  // don't want to close the file so we release the ownership right away
  CAutoFile f(m_file, SER_DISK, PROTOCOL_VERSION);
  f >> m_utxoSet;
  ++m_readTotal;
  --m_utxoSetLeft;
  f.release();
}

bool Iterator::MoveCursorTo(uint64_t utxoSetIndex) {
  if (m_indexer->GetMeta().m_totalUTXOSets <= utxoSetIndex) {
    return false;
  }

  // prevent reading the first message twice
  // when after the initialization MoveCursorTo(0) is invoked
  if (m_readTotal == 1 && utxoSetIndex == 0) {
    return true;
  }

  closeFile();

  m_file = m_indexer->GetClosestIdx(utxoSetIndex, m_utxoSetLeft, m_readTotal);
  if (!m_file) {
    return false;
  }

  Next();  // consume the message

  // keep reading more messages
  // until the index is equal to requested
  while (m_readTotal < utxoSetIndex + 1) {
    if (!Valid()) {
      return false;
    }
    Next();
  }

  return true;
}

bool Iterator::GetUTXOSets(uint64_t utxoSetIndex, uint16_t count,
                           std::vector<UTXOSet> &utxoSetOut) {
  if (!MoveCursorTo(utxoSetIndex)) {
    return false;
  }

  // todo(kostia): don't return more than 4MB as it's the maximum allowed
  // message size in P2P. 10K UTXO Sets is ~1MB and on Bitcoin data doesn't grow
  // more than 1.2MB but theoretically it can go beyond the 4MB limit

  utxoSetOut.clear();
  utxoSetOut.reserve(count);

  uint16_t n = 0;
  while (Valid()) {
    if (n >= count) {
      break;
    }

    utxoSetOut.emplace_back(GetUTXOSet());
    ++n;

    Next();
  }

  return true;
}

void Iterator::closeFile() {
  if (m_file) {
    fclose(m_file);
    m_file = nullptr;
  }
}

}  // namespace snapshot
