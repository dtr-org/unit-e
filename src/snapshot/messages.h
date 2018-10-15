// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_MESSAGES_H
#define UNITE_SNAPSHOT_MESSAGES_H

#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>
#include <coins.h>

namespace snapshot {

//! UTXOSubset type is used to transfer the snapshot over P2P and to store the
//! snapshot on disk too. It is used instead of UTXO because it has more compact
//! form, doesn't repeat TX specific fields for each output.
struct UTXOSubset {
  uint256 m_txId;
  uint32_t m_height;  // at which block height the TX was included
  bool m_isCoinBase;
  std::map<uint32_t, CTxOut> m_outputs;  // key is the CTxOut index

  UTXOSubset() : m_txId(), m_height(0), m_isCoinBase(false), m_outputs() {}

  UTXOSubset(uint256 txId, uint32_t height, bool isCoinBase,
             std::map<uint32_t, CTxOut> outMap)
      : m_txId(txId),
        m_height(height),
        m_isCoinBase(isCoinBase),
        m_outputs{std::move(outMap)} {}

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
//! m_utxoSubsetIndex = 0
//! m_utxoSubsetCount = >0
struct GetSnapshot {
  uint256 m_bestBlockHash;
  uint64_t m_utxoSubsetIndex;
  uint16_t m_utxoSubsetCount;

  GetSnapshot()
      : m_bestBlockHash(), m_utxoSubsetIndex(0), m_utxoSubsetCount(0) {}

  explicit GetSnapshot(const uint256 &bestBlockHash)
      : m_bestBlockHash(bestBlockHash),
        m_utxoSubsetIndex(0),
        m_utxoSubsetCount(0) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_bestBlockHash);
    READWRITE(m_utxoSubsetIndex);
    READWRITE(m_utxoSubsetCount);
  }
};

//! \brief Snapshot message is used to reply to GetSnapshot P2P request.
//!
//! When m_totalUTXOSubsets == m_utxoSubsetIndex + m_utxoSubsets.size() this
//! chunk is considered the last chunk of the snapshot.
struct Snapshot {
  uint256 m_snapshotHash;
  uint256 m_bestBlockHash;
  uint64_t m_totalUTXOSubsets;
  uint64_t m_utxoSubsetIndex;
  std::vector<UTXOSubset> m_utxoSubsets;

  Snapshot()
      : m_snapshotHash(),
        m_bestBlockHash(),
        m_totalUTXOSubsets(0),
        m_utxoSubsetIndex(0),
        m_utxoSubsets() {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_snapshotHash);
    READWRITE(m_bestBlockHash);
    READWRITE(m_totalUTXOSubsets);
    READWRITE(m_utxoSubsetIndex);
    READWRITE(m_utxoSubsets);
  }
};

//! UTXO is used to calculate the snapshot hash. It is used instead of more
//! compact UTXOSubset struct because it allows to add/subtract one output
//! without constructing the whole Tx which could be expensive (require lookup
//! to the disk)
struct UTXO {
  uint256 m_txId;
  uint32_t m_height;
  bool m_isCoinBase;
  uint32_t m_txOutIndex;
  CTxOut m_txOut;

  UTXO()
      : m_txId(),
        m_height(0),
        m_isCoinBase(false),
        m_txOutIndex(0),
        m_txOut() {}

  UTXO(COutPoint &out, Coin &coin)
      : m_txId(out.hash),
        m_height(coin.nHeight),
        m_isCoinBase(coin.IsCoinBase()),
        m_txOutIndex(out.n),
        m_txOut(coin.out) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_txId);
    READWRITE(m_height);
    READWRITE(m_isCoinBase);
    READWRITE(m_txOutIndex);
    READWRITE(m_txOut);
  }
};

}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_MESSAGES_H
