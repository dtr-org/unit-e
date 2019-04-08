// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_GRAPHENE_H
#define UNITE_P2P_GRAPHENE_H

#include <unordered_set>

#include <bloom.h>
#include <iblt.h>
#include <p2p/graphene_common.h>
#include <p2p/graphene_hasher.h>
#include <p2p/graphene_messages.h>
#include <primitives/block.h>
#include <serialize.h>
#include <txpool.h>

namespace p2p {

boost::optional<GrapheneBlock> CreateGrapheneBlock(const CBlock &block,
                                                   size_t sender_tx_count_wo_block,
                                                   size_t receiver_tx_count,
                                                   FastRandomContext &random);

// clang-format off
BETTER_ENUM(
  GrapheneDecodeState,
  uint8_t,
  HAS_ALL_TXS = 0,
  CANT_DECODE_IBLT = 1,
  NEED_MORE_TXS = 2
)
// clang-format on

class GrapheneBlockReconstructor {
 public:
  GrapheneBlockReconstructor(const GrapheneBlock &graphene_block, const TxPool &tx_pool);

  void AddMissingTxs(const std::vector<CTransactionRef> &txs);

  GrapheneDecodeState GetState() const;

  CBlock ReconstructLTOR() const;

  const std::set<GrapheneShortHash> &GetMissingShortTxHashes() const;

  uint256 GetBlockHash() const;

 private:
  CBlockHeader m_header;
  std::set<GrapheneShortHash> m_missing_short_tx_hashes;
  std::vector<CTransactionRef> m_decoded_txs;
  std::vector<CTransactionRef> m_prefilled_txs;
  GrapheneDecodeState m_state = GrapheneDecodeState::CANT_DECODE_IBLT;
  GrapheneHasher m_hasher;
};

struct GrapheneBlockParams {
  GrapheneBlockParams(size_t expected_symmetric_difference,
                      size_t bloom_entries_num,
                      double bloom_filter_fpr);

  const size_t expected_symmetric_difference;
  const size_t bloom_entries_num;
  const double bloom_filter_fpr;
};

GrapheneBlockParams OptimizeGrapheneBlockParams(size_t block_txs,
                                                size_t all_sender_txs,
                                                size_t all_receiver_txs);

}  // namespace p2p

#endif  //UNITE_P2P_GRAPHENE_H
