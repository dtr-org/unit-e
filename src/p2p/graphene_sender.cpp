// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/graphene_sender.h>

#include <blockencodings.h>
#include <chainparams.h>
#include <net_processing.h>
#include <p2p/graphene.h>
#include <tinyformat.h>
#include <util/scope_stopwatch.h>
#include <validation.h>

namespace p2p {

class DisabledGrapheneSender : public GrapheneSender {
  void UpdateRequesterTxPoolCount(const CNode &requester, uint64_t new_count) override {}
  bool SendBlock(CNode &to, const CBlock &block, const CBlockIndex &index) override {
    return false;
  }

  void OnGrapheneTxRequestReceived(CNode &from,
                                   const GrapheneTxRequest &request) override {
    LogPrint(BCLog::NET, "Graphene block tx is requested in violation of protocol, peer %d\n", from.GetId());
    Misbehaving(from.GetId(), 100);
  }

  void OnDisconnected(NodeId node) override {}
};

class GrapheneSenderImpl : public GrapheneSender {
 public:
  explicit GrapheneSenderImpl(Dependency<::TxPool> tx_pool);
  void UpdateRequesterTxPoolCount(const CNode &requester, uint64_t new_count) override;
  bool SendBlock(CNode &to, const CBlock &block, const CBlockIndex &index) override;
  void OnGrapheneTxRequestReceived(CNode &from,
                                   const GrapheneTxRequest &request) override;
  void OnDisconnected(NodeId node) override;

 private:
  struct ReceiverInfo {
    int last_requested_height = 0;
    uint256 last_requested_hash;
    bool requested_tx = false;
    uint64_t tx_pool_count = 0;
    uint64_t last_nonce = 0;
  };

  CCriticalSection m_cs;
  std::unordered_map<NodeId, ReceiverInfo> m_receiver_infos;
  Dependency<TxPool> m_sender_tx_pool;
  FastRandomContext m_random;
};

GrapheneSenderImpl::GrapheneSenderImpl(Dependency<::TxPool> tx_pool)
    : m_sender_tx_pool(tx_pool),
      m_random(false) {
}

void GrapheneSenderImpl::UpdateRequesterTxPoolCount(const CNode &requester,
                                                    uint64_t new_count) {
  LOCK(m_cs);
  m_receiver_infos[requester.GetId()].tx_pool_count = new_count;
}

bool GrapheneSenderImpl::SendBlock(CNode &to, const CBlock &block, const CBlockIndex &index) {
  if (block.vtx.size() < MIN_TRANSACTIONS_IN_GRAPHENE_BLOCK) {
    return false;
  }

  LOCK(m_cs);

  const auto it = m_receiver_infos.find(to.GetId());

  assert(it != m_receiver_infos.end());

  ReceiverInfo &receiver_info = it->second;

  if (index.nHeight <= receiver_info.last_requested_height) {
    // Graphene block is more expensive to construct than usual block or
    // compact block, and unlike compact blocks - you can not cache graphene
    // blocks effectively

    LogPrint(BCLog::NET, "Peer %d requested too old graphene block\n", to.GetId());
    return false;
  }

  receiver_info.last_requested_height = index.nHeight;
  receiver_info.last_requested_hash = block.GetHash();
  receiver_info.requested_tx = false;

  LogPrint(BCLog::NET, "Constructing graphene block %s for peer %d, txpool size %d\n",
           block.GetHash().GetHex(), to.GetId(), receiver_info.tx_pool_count);

  // Graphene block might not be constructed if we think it is improbable for
  // receiver to decode it, for example
  auto maybe_graphene_block = CreateGrapheneBlock(block,
                                                  m_sender_tx_pool->GetTxCount(),
                                                  receiver_info.tx_pool_count,
                                                  m_random);
  if (!maybe_graphene_block) {
    return false;
  }

  GrapheneBlock &graphene_block = maybe_graphene_block.get();

  receiver_info.last_nonce = graphene_block.nonce;

  {
    SCOPE_STOPWATCH("Compare graphene and compact block sizes");

    const size_t graphene_block_size = GetSerializeSize(graphene_block, SER_NETWORK, PROTOCOL_VERSION);

    // This can be optimized if one day we notice performance problems here

    // We assume that ALL unit-e nodes support compact blocks and all compact
    // blocks are smaller than legacy blocks
    CBlockHeaderAndShortTxIDs cmpct_block(block);
    const size_t cmpct_block_size = GetSerializeSize(cmpct_block, SER_NETWORK, PROTOCOL_VERSION);

    if (graphene_block_size >= cmpct_block_size) {
      LogPrint(BCLog::NET, "Graphene block %s is bigger than compact block (%d vs %d bytes)\n",
               block.GetHash().GetHex(), graphene_block_size, cmpct_block_size);

      return false;
    }
  }

  LogPrint(BCLog::NET, "Sending graphene block %s to peer %d\n",
           block.GetHash().GetHex(), to.GetId());

  PushMessage(to, NetMsgType::GRAPHENEBLOCK, std::move(graphene_block));
  return true;
}

void GrapheneSenderImpl::OnGrapheneTxRequestReceived(CNode &from,
                                                     const GrapheneTxRequest &request) {

  if (request.block_hash.IsNull() || request.missing_tx_short_hashes.empty()) {
    LogPrint(BCLog::NET, "Received incorrect graphene tx request from peer %d\n", from.GetId());
    Misbehaving(from.GetId(), 100);
    return;
  }

  uint64_t nonce;

  {
    LOCK(m_cs);
    const auto it = m_receiver_infos.find(from.GetId());
    if (it == m_receiver_infos.end() || it->second.last_requested_hash != request.block_hash) {
      LogPrint(BCLog::NET, "Peer %d requested graphene tx for block we didn't send to it (%s)\n",
               from.GetId(), request.block_hash.GetHex());
      Misbehaving(from.GetId(), 10);
      return;
    }

    if (it->second.requested_tx) {
      LogPrint(BCLog::NET, "Peer %d has already requested graphene tx for block %s\n",
               from.GetId(), request.block_hash.GetHex());
      Misbehaving(from.GetId(), 10);
      return;
    }
    it->second.requested_tx = true;
    nonce = it->second.last_nonce;
  }

  FUNCTION_STOPWATCH();
  LOCK(cs_main);

  const uint256 block_hash = request.block_hash;

  const CBlockIndex *block_index = LookupBlockIndex(request.block_hash);
  assert(block_index); // We recently checked that WE sent this block first

  GrapheneTx response(block_hash, {});

  {
    SCOPE_STOPWATCH("Load block and collect missing txs");

    CBlock block;

    if (!ReadBlockFromDisk(block, block_index, Params().GetConsensus())) {
      LogPrint(BCLog::NET, "Can not read block %s from disk\n", block_hash.GetHex());
      return;
    }

    GrapheneHasher hasher(block, nonce);

    for (const auto &tx : block.vtx) {
      const GrapheneShortHash short_hash = hasher.GetShortHash(*tx);

      if (request.missing_tx_short_hashes.count(short_hash)) {
        response.txs.emplace_back(tx);
      }
    }
  }

  PushMessage(from, NetMsgType::GRAPHENETX, response);
}

void GrapheneSenderImpl::OnDisconnected(NodeId node) {
  LOCK(m_cs);
  m_receiver_infos.erase(node);
}

std::unique_ptr<GrapheneSender> GrapheneSender::New(Dependency<ArgsManager> args,
                                                    Dependency<TxPool> txpool) {

  const bool enabled = args->GetBoolArg("-graphene", true);
  if (enabled) {
    return MakeUnique<GrapheneSenderImpl>(txpool);
  }

  return MakeUnique<DisabledGrapheneSender>();
}

}  // namespace p2p
