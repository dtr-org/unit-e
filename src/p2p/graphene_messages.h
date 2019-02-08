// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_GRAPHENE_MESSAGES_H
#define UNITE_P2P_GRAPHENE_MESSAGES_H

#include <bloom.h>
#include <iblt.h>
#include <p2p/graphene_common.h>
#include <primitives/block.h>
#include <serialize.h>
#include <unordered_set>
#include <utility>

namespace p2p {

class GrapheneBlockRequest {
 public:
  uint256 requested_block_hash;
  uint64_t requester_mempool_count;

  GrapheneBlockRequest() = default;

  GrapheneBlockRequest(const uint256 &requested_block_hash,
                       uint64_t requester_mempool_count)
      : requested_block_hash(requested_block_hash),
        requester_mempool_count(requester_mempool_count) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(requested_block_hash);
    READWRITE(requester_mempool_count);
  }
};

class GrapheneBlock {
 public:
  CBlockHeader header;
  uint64_t nonce;
  CBloomFilter bloom_filter;
  GrapheneIblt iblt;
  std::vector<CTransactionRef> prefilled_transactions;
  GrapheneBlock(const CBlockHeader &header,
                const uint64_t nonce,
                CBloomFilter bloom_filter,
                GrapheneIblt iblt,
                std::vector<CTransactionRef> prefilled_transactions)
      : header(header),
        nonce(nonce),
        bloom_filter(std::move(bloom_filter)),
        iblt(std::move(iblt)),
        prefilled_transactions(std::move(prefilled_transactions)) {}

  GrapheneBlock() = default;

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(header);
    READWRITE(nonce);
    READWRITE(bloom_filter);
    READWRITE(iblt);
    READWRITE(prefilled_transactions);
  }
};

class GrapheneTxRequest {
 public:
  uint256 block_hash;
  std::set<GrapheneShortHash> missing_tx_short_hashes;

  GrapheneTxRequest() = default;

  GrapheneTxRequest(const uint256 &block_hash,
                    std::set<GrapheneShortHash> missing_tx_short_hashes)
      : block_hash(block_hash),
        missing_tx_short_hashes(std::move(missing_tx_short_hashes)) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(block_hash);
    READWRITE(missing_tx_short_hashes);
  }
};

class GrapheneTx {
 public:
  uint256 block_hash;
  std::vector<CTransactionRef> txs;

  GrapheneTx() = default;

  GrapheneTx(const uint256 &block_hash, std::vector<CTransactionRef> txs)
      : block_hash(block_hash), txs(std::move(txs)) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(block_hash);
    READWRITE(txs);
  }
};

}  // namespace p2p

#endif  //UNITE_P2P_GRAPHENE_MESSAGES_H
