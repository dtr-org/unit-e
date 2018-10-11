// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_MESSAGES_H
#define UNITE_SNAPSHOT_MESSAGES_H

#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>

namespace snapshot {

using TxOutIndex = uint32_t;

//! \brief A TxUTXOSet it a subset of the global utxo set for a given Tx.
//!
//! Storing each UTXO individually including TxId wastes memory/storage as
//! UTXOs are grouped within a Transaction and thus share the same TxId. A
//! TxUTXOSet is the set of UTXOs that belong to a specific transaction.
struct TxUTXOSet {

  //! The transaction id for all the UTXOs in this TxTxUTXOSet
  uint256 m_txId;

  //! The height of the block in which the transaction was included
  uint32_t m_height;

  //! Whether the transaction which harbours this utxo set was a coinbase tx.
  bool m_isCoinBase;

  //! The set of UTXOs for this transaction, mapped by their index within it.
  std::map<TxOutIndex, CTxOut> m_outputs;

  TxUTXOSet() : m_txId(), m_height(0), m_isCoinBase(false), m_outputs() {}

  TxUTXOSet(uint256 txId, uint32_t height, bool isCoinBase,
            std::map<TxOutIndex, CTxOut> outMap)
      : m_txId(txId),
        m_height(height),
        m_isCoinBase(isCoinBase),
        m_outputs(std::move(outMap)) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_txId);
    READWRITE(m_height);
    READWRITE(m_isCoinBase);
    READWRITE(m_outputs);
  }
};

//! \brief GetSnapshot is a message to request the snapshot chunk.
//!
//! During the initial request to peers it has the following values:
//! bestBlockHash = empty
//! m_utxoSetIndex = 0
//! m_utxoSetCount = >0
struct GetSnapshot {
  uint256 m_bestBlockHash;
  uint64_t m_utxoSetIndex;
  uint16_t m_utxoSetCount;

  GetSnapshot() : m_bestBlockHash(), m_utxoSetIndex(0), m_utxoSetCount(0) {}

  explicit GetSnapshot(const uint256 &bestBlockHash)
      : m_bestBlockHash(bestBlockHash), m_utxoSetIndex(0), m_utxoSetCount(0) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_bestBlockHash);
    READWRITE(m_utxoSetIndex);
    READWRITE(m_utxoSetCount);
  }
};

//! \brief Snapshot message is used to reply to GetSnapshot P2P request.
//!
//! When m_totalTxUTXOSets == m_utxoSetIndex + m_utxoSets.size() this chunk is
//! considered the last chunk of the snapshot.
struct Snapshot {
  uint256 m_snapshotHash;
  uint256 m_bestBlockHash;
  uint64_t m_totalTxUTXOSets;
  uint64_t m_utxoSetIndex;
  std::vector<TxUTXOSet> m_utxoSets;

  Snapshot()
      : m_snapshotHash(),
        m_bestBlockHash(),
        m_totalTxUTXOSets(0),
        m_utxoSetIndex(0),
        m_utxoSets() {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_snapshotHash);
    READWRITE(m_bestBlockHash);
    READWRITE(m_totalTxUTXOSets);
    READWRITE(m_utxoSetIndex);
    READWRITE(m_utxoSets);
  }
};

}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_MESSAGES_H
