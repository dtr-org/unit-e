// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/chainstate_iterator.h>

#include <coins.h>

namespace snapshot {

ChainstateIterator::ChainstateIterator(CCoinsViewDB *view)
    : m_valid(true), m_cursor(view->Cursor()) {
  Next();
}

bool ChainstateIterator::Valid() { return m_valid; }

void ChainstateIterator::Next() {
  bool stop = false;
  while (m_cursor->Valid()) {
    COutPoint key;
    Coin coin;
    if (m_cursor->GetKey(key) && m_cursor->GetValue(coin)) {
      if (!m_outputs.empty() && key.hash != m_prev_tx_id) {
        m_utxo_subset =
            UTXOSubset(m_prev_tx_id, m_prev_coin.nHeight, m_prev_coin.tx_type,
                       std::move(m_outputs));
        m_outputs.clear();
        stop = true;
      }

      m_outputs[key.n] = coin.out;
      m_prev_coin = coin;
      m_prev_tx_id = key.hash;
    }

    m_cursor->Next();

    if (stop) {
      return;
    }
  }

  // we reached the end of the out chainstate. construct the last UTXO subset

  if (m_outputs.empty()) {
    m_valid = false;
    return;
  }

  m_utxo_subset = UTXOSubset(m_prev_tx_id, m_prev_coin.nHeight,
                             m_prev_coin.tx_type, std::move(m_outputs));
  m_outputs.clear();
}

}  // namespace snapshot
