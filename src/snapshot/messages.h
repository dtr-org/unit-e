// Copyright (c) 2018 The Unit-e developers
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
  uint256 tx_id;

  //! at which block height the TX was included
  uint32_t height;

  bool is_coin_base;

  //! key is the CTxOut index
  std::map<uint32_t, CTxOut> outputs;

  UTXOSubset() : tx_id(), height(0), is_coin_base(false), outputs() {}

  UTXOSubset(uint256 _tx_id, uint32_t _height, bool _is_coin_base,
             std::map<uint32_t, CTxOut> out_map)
      : tx_id(_tx_id),
        height(_height),
        is_coin_base(_is_coin_base),
        outputs{std::move(out_map)} {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(tx_id);
    READWRITE(height);
    READWRITE(is_coin_base);
    READWRITE(outputs);
  }
};

//! \brief message to discover the best snapshot
struct BestSnapshot {
  uint256 snapshot_hash;
  uint256 block_hash;
  uint256 stake_modifier;
  uint64_t total_utxo_subsets = 0;

  BestSnapshot() = default;

  bool operator==(const BestSnapshot &other) const {
    return snapshot_hash == other.snapshot_hash;
  }

  bool operator!=(const BestSnapshot &other) const {
    return !(*this == other);
  }

  bool IsNull() const {
    return snapshot_hash.IsNull();
  }

  void SetNull() {
    snapshot_hash.SetNull();
    block_hash.SetNull();
    stake_modifier.SetNull();
    total_utxo_subsets = 0;
  }

  ADD_SERIALIZE_METHODS;
  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(snapshot_hash);
    READWRITE(block_hash);
    READWRITE(stake_modifier);
    READWRITE(total_utxo_subsets);
  }
};

//! \brief message to request the snapshot chunk
struct GetSnapshot {
  uint256 snapshot_hash;
  uint64_t utxo_subset_index = 0;
  uint16_t utxo_subset_count = 0;

  GetSnapshot() = default;

  explicit GetSnapshot(const uint256 &_snapshot_hash)
      : snapshot_hash(_snapshot_hash),
        utxo_subset_index(0),
        utxo_subset_count(0) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(snapshot_hash);
    READWRITE(utxo_subset_index);
    READWRITE(utxo_subset_count);
  }
};

//! \brief message is used to reply to GetSnapshot P2P request
//!
//! When total_utxo_subsets == utxo_subset_index + utxo_subsets.size() this
//! chunk is considered the last chunk of the snapshot.
struct Snapshot {
  uint256 snapshot_hash;
  uint64_t utxo_subset_index;
  std::vector<UTXOSubset> utxo_subsets;

  Snapshot()
      : snapshot_hash(),
        utxo_subset_index(0),
        utxo_subsets() {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(snapshot_hash);
    READWRITE(utxo_subset_index);
    READWRITE(utxo_subsets);
  }
};

//! UTXO is a representation of a single output and used to calculate the
//! snapshot hash. Coin class (which has the same schema) is not used as it
//! doesn't follow the P2P serialization convention.
struct UTXO {
  COutPoint out_point;
  uint32_t height;
  bool is_coin_base;
  CTxOut tx_out;

  UTXO()
      : out_point(),
        height(0),
        is_coin_base(false),
        tx_out() {}

  UTXO(const COutPoint &out, const Coin &coin);

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(out_point);
    READWRITE(height);
    READWRITE(is_coin_base);
    READWRITE(tx_out);
  }
};

class SnapshotHash {
 public:
  SnapshotHash();
  explicit SnapshotHash(const std::vector<uint8_t> &data);

  void AddUTXO(const UTXO &utxo);
  void SubtractUTXO(const UTXO &utxo);

  //! GetHash returns the hash that represents the snapshot
  //!
  //! \param stakeModifier which points to the same height as the snapshot hash
  //! \return the hash which is stored inside the coinstake TX
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
