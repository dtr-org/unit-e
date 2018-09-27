// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_P2P_MESSAGES_H
#define UNITE_SNAPSHOT_P2P_MESSAGES_H

#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>

namespace snapshot {

// Utx type is used to store Utx records on the disk too
struct Utx {
  uint256 hash;
  uint32_t height;  // at which block height the TX was included
  bool isCoinBase;
  std::map<uint32_t, CTxOut> outputs;  // key is the CTxOut index

  Utx() : hash{}, height{0}, isCoinBase{false}, outputs{} {}

  Utx(uint256 hash_, uint32_t height_, bool isCoinBase_,
      std::map<uint32_t, CTxOut> outMap)
      : hash{hash_},
        height{height_},
        isCoinBase{isCoinBase_},
        outputs{std::move(outMap)} {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(hash);
    READWRITE(height);
    READWRITE(isCoinBase);
    READWRITE(outputs);
  }
};

//! P2pGetSnapshot message is used to request the snapshot chunk.
//!
//! During the initial request to peers it has the following values:
//! bestBlockHash = empty
//! utxIndex = 0
//! utxCount = >0
struct P2pGetSnapshot {
  uint256 bestBlockHash;
  uint64_t utxIndex;
  uint16_t utxCount;

  P2pGetSnapshot() : bestBlockHash{}, utxIndex{0}, utxCount{0} {}

  explicit P2pGetSnapshot(const uint256 &hash)
      : bestBlockHash{hash}, utxIndex{0}, utxCount{0} {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(bestBlockHash);
    READWRITE(utxIndex);
    READWRITE(utxCount);
  }
};

//! P2pSnapshot message is used to reply to P2pGetSnapshot request.
//!
//! When totalUtxs == utxIndex + utxs.size() this chunk is considered
//! the last chunk of the snapshot.
struct P2pSnapshot {
  uint256 snapshotHash;
  uint256 bestBlockHash;
  uint64_t totalUtxs;
  uint64_t utxIndex;
  std::vector<Utx> utxs;

  P2pSnapshot()
      : snapshotHash{}, bestBlockHash{}, totalUtxs{0}, utxIndex{0}, utxs{} {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(snapshotHash);
    READWRITE(bestBlockHash);
    READWRITE(totalUtxs);
    READWRITE(utxIndex);
    READWRITE(utxs);
  }
};

}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_P2P_MESSAGES_H
