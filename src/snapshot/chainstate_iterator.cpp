// Copyright (c) 2018 The unit-e core developers
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
      if (!m_outputs.empty() && key.hash != m_prevTxId) {
        m_utxoSubset =
            UTXOSubset(m_prevTxId, m_prevCoin.nHeight, m_prevCoin.IsCoinStake(),
                       std::move(m_outputs));
        m_outputs.clear();
        stop = true;
      }

      m_outputs[key.n] = coin.out;
      m_prevCoin = coin;
      m_prevTxId = key.hash;
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

  m_utxoSubset = UTXOSubset(m_prevTxId, m_prevCoin.nHeight,
                            m_prevCoin.IsCoinStake(), std::move(m_outputs));
  m_outputs.clear();
}

}  // namespace snapshot
