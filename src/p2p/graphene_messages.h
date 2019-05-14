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

struct GrapheneBlockRequest {
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

struct GrapheneBlock {
  CBlockHeader header;
  uint64_t nonce;
  CBloomFilter bloom_filter;
  GrapheneIblt iblt;
  std::vector<CTransactionRef> prefilled_transactions;
  //! signature of the block for Proof-of-Stake
  std::vector<uint8_t> signature;

  GrapheneBlock(const CBlock &block,
                const uint64_t nonce,
                CBloomFilter bloom_filter,
                GrapheneIblt iblt,
                std::vector<CTransactionRef> prefilled_transactions)
      : header(block.GetBlockHeader()),
        nonce(nonce),
        bloom_filter(std::move(bloom_filter)),
        iblt(std::move(iblt)),
        prefilled_transactions(std::move(prefilled_transactions)),
        signature(block.signature) {}

  GrapheneBlock() = default;

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(header);
    READWRITE(nonce);
    READWRITE(bloom_filter);
    READWRITE(iblt);
    READWRITE(prefilled_transactions);
    READWRITE(signature);
  }
};

struct GrapheneTxRequest {
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

struct GrapheneTx {
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
