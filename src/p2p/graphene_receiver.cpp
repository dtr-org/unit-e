// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <net.h>
#include <net_processing.h>
#include <p2p/graphene.h>
#include <p2p/graphene_receiver.h>
#include <sync.h>
#include <tinyformat.h>
#include <util.h>
#include <util/scope_stopwatch.h>
#include <validation.h>

namespace p2p {

GrapheneReceiver::GrapheneReceiver(const bool enabled,
                                   Dependency<TxPool> txpool)
    : m_enabled(enabled),
      m_txpool(txpool) {}

void GrapheneReceiver::BeforeBlocksRequested(CNode &from,
                                             std::vector<CInv> &invs,
                                             const CBlockIndex &last_inv_block_index,
                                             const size_t blocks_in_flight) {

  if (!m_enabled) {
    return;
  }

  // Copying similar logic from compact block
  // This also technically means that we can request only one graphene block at a time
  if (invs.size() != 1 ||
      blocks_in_flight != 1 ||
      !last_inv_block_index.pprev->IsValid(BLOCK_VALID_CHAIN)) {
    return;
  }

  uint256 block_hash = invs.front().hash;
  invs.erase(invs.begin());

  {
    LOCK(m_cs);

    if (!m_graphene_blocks_in_flight.empty()) {
      LogPrintf("WARN: graphene blocks in flight state is out of sync with global blocks in flight");
      m_graphene_blocks_in_flight.clear();
    }
    m_graphene_blocks_in_flight.emplace(block_hash, BlockReceiveState(block_hash, from.GetId()));
  }

  GrapheneBlockRequest request(block_hash, m_txpool->GetTxCount());

  LogPrint(BCLog::NET, "Requesting graphene block %s from peer=%d\n",
           request.requested_block_hash.GetHex(), from.GetId());

  PushMessage(from, NetMsgType::GETGRAPHENE, request);
}

void GrapheneReceiver::OnGrapheneBlockReceived(CNode &from,
                                               const GrapheneBlock &graphene_block) {

  const auto block_hash = graphene_block.header.GetHash();

  if (!m_enabled) {
    LogPrint(BCLog::NET, "Graphene block %s sent in violation of protocol, peer=%d\n",
             block_hash.GetHex(), from.GetId());
    Misbehaving(from.GetId(), 100);
    return;
  }

  if (!graphene_block.iblt.IsValid()) {
    LogPrint(BCLog::NET, "Iblt in graphene block %s is invalid, peer=%d\n", block_hash.GetHex(), from.GetId());

    Misbehaving(from.GetId(), 100);
    MarkBlockNotInFlight(from, block_hash);
    return;
  }

  const size_t txs_in_block = graphene_block.iblt.Size();

  if (txs_in_block > MAX_TRANSACTIONS_IN_GRAPHENE_BLOCK) {
    LogPrint(BCLog::NET, "Too many transactions(%d) in a graphene block %s from peer=%d\n",
             txs_in_block, block_hash.GetHex(), from.GetId());
    Misbehaving(from.GetId(), 100);
    MarkBlockNotInFlight(from, block_hash);
    return;
  }

  CValidationState val_state;

  if (!AcceptBlockHeader(graphene_block.header, val_state, Params(), nullptr)) {
    LogPrint(BCLog::NET, "Received invalid graphene block %s from peer=%d\n",
             block_hash.GetHex(), from.GetId());

    int dos_score;
    if (val_state.IsInvalid(dos_score)) {
      Misbehaving(from.GetId(), dos_score);
    }

    MarkBlockNotInFlight(from, block_hash);
    return;
  }

  std::unique_ptr<GrapheneBlockReconstructor> reconstructor;

  {
    LOCK(m_cs);

    if (!IsInFlight(block_hash, from.GetId())) {
      // Graphene blocks are parametrized with receiver tx poool size,
      // If we haven't requested this block => we never sent this size => we have
      // very high chance this incoming block won't decode. Don't want to spend
      // resources on it
      LogPrint(BCLog::NET, "Graphene block %s from peer %d was not requested\n",
               block_hash.GetHex(), from.GetId());
      Misbehaving(from.GetId(), 20);

      return;
    }

    const auto it = m_graphene_blocks_in_flight.find(block_hash);

    if (it->second.reconstructor) {
      LogPrint(BCLog::NET, "Received graphene block %s from peer=%d, but graphene tx was expected\n",
               block_hash.GetHex(), from.GetId());
      Misbehaving(from.GetId(), 20);
      MarkBlockNotInFlight(from, block_hash);
      return;
    }

    LogPrint(BCLog::NET, "Received graphene block %s from peer=%d\n",
             block_hash.GetHex(), from.GetId());

    reconstructor = MakeUnique<GrapheneBlockReconstructor>(graphene_block, *m_txpool);

    const auto reconstructor_state = reconstructor->GetState();

    switch (reconstructor_state) {
      case GrapheneDecodeState::HAS_ALL_TXS: {
        // Will be handled below, as soon as we release critical section
        break;
      }
      case GrapheneDecodeState::CANT_DECODE_IBLT: {
        LogPrint(BCLog::NET, "Unable to decode iblt in graphene block %s\n",
                 block_hash.GetHex());
        RequestFallbackBlock(from, block_hash);
        return;
      }
      case GrapheneDecodeState::NEED_MORE_TXS: {
        GrapheneTxRequest request(block_hash, reconstructor->GetMissingShortTxHashes());

        LogPrint(BCLog::NET, "Graphene block %s reconstructed, but %d transactions are missing\n",
                 block_hash.GetHex(), request.missing_tx_short_hashes.size());

        PushMessage(from, NetMsgType::GETGRAPHENETX, std::move(request));
        it->second.reconstructor = std::move(reconstructor);
        return;
      }
      default:
        throw std::runtime_error(reconstructor_state._to_string() + std::string(" is not handled"));
    }
  }

  ReconstructAndSubmitBlock(from, *reconstructor);
}

void GrapheneReceiver::ReconstructAndSubmitBlock(CNode &from,
                                                 const GrapheneBlockReconstructor &reconstructor) {
  // Might cause a deadlock otherwise
  AssertLockNotHeld(m_cs);

  const uint256 block_hash = reconstructor.GetBlockHash();
  CBlock block = reconstructor.ReconstructLTOR();

  if (CheckMerkleRoot(block)) {
    LogPrint(BCLog::NET, "Graphene block %s is valid. Submitting\n",
             block_hash.GetHex());

    MarkBlockNotInFlight(from, block_hash);

    {
      LOCK(cs_main);
      // This map is used ProcessNewBlock and its descendants to determine
      // source of block and to ban it if block is invalid
      mapBlockSource.emplace(block_hash, std::make_pair(from.GetId(), true));
    }

    ProcessNewBlock(Params(), std::make_shared<CBlock>(std::move(block)), true, nullptr);

    {
      LOCK(cs_main);
      // ProcessNewBlock might exit before mapBlockSource is cleaned
      mapBlockSource.erase(block_hash);
    }
  } else {
    LogPrint(BCLog::NET, "Graphene block's (%s) merkle root is invalid. Requesting fallback\n",
             block_hash.GetHex());
    RequestFallbackBlock(from, block_hash);
  }
}

void GrapheneReceiver::OnGrapheneTxReceived(CNode &from,
                                            const GrapheneTx &graphene_tx) {

  const uint256 &block_hash = graphene_tx.block_hash;

  if (block_hash.IsNull() || graphene_tx.txs.empty()) {
    LogPrint(BCLog::NET, "Received incorrect graphene tx from peer=%d\n", from.GetId());
    Misbehaving(from.GetId(), 100);
    return;
  }

  std::unique_ptr<GrapheneBlockReconstructor> reconstructor;

  {
    LOCK(m_cs);

    if (!IsInFlight(block_hash, from.GetId())) {
      LogPrint(BCLog::NET, "Peer %d sent us graphene block transactions for block we weren't expecting(%s)\n",
               from.GetId(), block_hash.GetHex());

      Misbehaving(from.GetId(), 20);

      return;
    }

    const auto it = m_graphene_blocks_in_flight.find(block_hash);
    BlockReceiveState &state = it->second;

    LogPrint(BCLog::NET, "Received graphene tx for block %s, peer=%d\n",
             block_hash.GetHex(), from.GetId());

    if (graphene_tx.txs.size() > state.reconstructor->GetMissingShortTxHashes().size()) {
      LogPrint(BCLog::NET, "Peer=%d sent us too many graphene txs for block %s\n",
               from.GetId(), block_hash.GetHex());

      Misbehaving(from.GetId(), 20);
      return;
    }

    state.reconstructor->AddMissingTxs(graphene_tx.txs);

    if (state.reconstructor->GetState() == +GrapheneDecodeState::HAS_ALL_TXS) {
      reconstructor = std::move(state.reconstructor);
    } else {
      LogPrint(BCLog::NET, "Can not reconstruct graphene block %s. Requesting fallback, peer=%d\n",
               block_hash.GetHex(), from.GetId());
      RequestFallbackBlock(from, block_hash);

      return;
    }
  }

  if (reconstructor) {
    ReconstructAndSubmitBlock(from, *reconstructor);
  }
}

void GrapheneReceiver::RequestFallbackBlock(CNode &from, const uint256 &block_hash) {
  {
    LOCK(m_cs);

    assert(IsInFlight(block_hash, from.GetId()));

    m_graphene_blocks_in_flight.erase(block_hash);
  }

  std::vector<CInv> invs{CInv(MSG_CMPCT_BLOCK, block_hash)};
  PushMessage(from, NetMsgType::GETDATA, invs);
}

bool GrapheneReceiver::CheckMerkleRoot(const CBlock &block) {
  bool mutated;
  const uint256 merkle_root = BlockMerkleRoot(block, &mutated);

  return !mutated && block.hashMerkleRoot == merkle_root;
}

void GrapheneReceiver::OnDisconnected(const NodeId node) {
  LOCK(m_cs);

  // We expect to not have many such blocks (in fact currently one), so linear scan is acceptable
  for (auto it = m_graphene_blocks_in_flight.begin(); it != m_graphene_blocks_in_flight.end();) {
    if (it->second.sender == node) {
      it = m_graphene_blocks_in_flight.erase(it++);
    } else {
      ++it;
    }
  }
}

void GrapheneReceiver::MarkBlockNotInFlight(const CNode &from,
                                            const uint256 &block_hash) {
  {
    LOCK(m_cs);
    if (!IsInFlight(block_hash, from.GetId())) {
      return;
    }
    m_graphene_blocks_in_flight.erase(block_hash);
  }

  LOCK(cs_main);

  // This is bitcoin housekeeping, if we won't properly update it, bitcoin might
  // not download compact blocks for example
  MarkBlockAsReceived(block_hash);
}

bool GrapheneReceiver::IsInFlight(const uint256 &block_hash, const NodeId sender) {
  LOCK(m_cs);

  const auto it = m_graphene_blocks_in_flight.find(block_hash);
  if (it == m_graphene_blocks_in_flight.end()) {
    return false;
  }

  return it->second.sender == sender;
}

std::unique_ptr<GrapheneReceiver> GrapheneReceiver::New(Dependency<::ArgsManager> args,
                                                        Dependency<::TxPool> txpool) {
  const bool enabled = args->GetBoolArg("-graphene", true);
  return MakeUnique<GrapheneReceiver>(enabled, txpool);
}

}  // namespace p2p
