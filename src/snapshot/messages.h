// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_MESSAGES_H
#define UNITE_SNAPSHOT_MESSAGES_H

#include <vector>

#include <primitives/transaction.h>
#include <secp256k1_multiset.h>
#include <serialize.h>
#include <uint256.h>

class Coin;

namespace snapshot {

//! UTXOSubset type is used to transfer the snapshot over P2P and to store the
//! snapshot on disk too. It is used instead of UTXO because it has more compact
//! form, doesn't repeat TX specific fields for each output.
struct UTXOSubset {
  uint256 m_txId;

  //! at which block height the TX was included
  uint32_t m_height;

  bool m_isCoinBase;

  //! key is the CTxOut index
  std::map<uint32_t, CTxOut> m_outputs;

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
  uint256 m_stakeModifier;
  uint64_t m_totalUTXOSubsets;
  uint64_t m_utxoSubsetIndex;
  std::vector<UTXOSubset> m_utxoSubsets;

  Snapshot()
      : m_snapshotHash(),
        m_bestBlockHash(),
        m_stakeModifier(),
        m_totalUTXOSubsets(0),
        m_utxoSubsetIndex(0),
        m_utxoSubsets() {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_snapshotHash);
    READWRITE(m_bestBlockHash);
    READWRITE(m_stakeModifier);
    READWRITE(m_totalUTXOSubsets);
    READWRITE(m_utxoSubsetIndex);
    READWRITE(m_utxoSubsets);
  }
};

//! UTXO is a representation of a single output and used to calculate the
//! snapshot hash. Coin class (which has the same schema) is not used as it
//! doesn't follow the P2P serialization convention.
struct UTXO {
  COutPoint m_outPoint;
  uint32_t m_height;
  bool m_isCoinBase;
  CTxOut m_txOut;

  UTXO()
      : m_outPoint(),
        m_height(0),
        m_isCoinBase(false),
        m_txOut() {}

  UTXO(const COutPoint &out, const Coin &coin);

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_outPoint);
    READWRITE(m_height);
    READWRITE(m_isCoinBase);
    READWRITE(m_txOut);
  }
};

class SnapshotHash {
 public:
  SnapshotHash();
  explicit SnapshotHash(const std::vector<uint8_t> &data);

  void AddUTXO(const UTXO &utxo);
  void SubtractUTXO(const UTXO &utxo);

  //! GetHash returns the hash that represents the snapshot
  //! and must be stored inside CoinBase TX.
  uint256 GetHash(uint256 stakeModifier) const;

  //! GetHashVector is a proxy to GetHash
  std::vector<uint8_t> GetHashVector(uint256 stakeModifier) const;

  void Clear();

  //! GetData returns internals of the hash and used to restore the state.
  std::vector<uint8_t> GetData() const;

 private:
  secp256k1_multiset m_multiset;
};

//! InitSecp256k1Context creates secp256k1_context. If creation failed,
//! the node must be stopped
bool InitSecp256k1Context();

//! DestroySecp256k1Context destroys secp256k1_context. Must be invoked before
//! creating a new context
void DestroySecp256k1Context();

}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_MESSAGES_H
