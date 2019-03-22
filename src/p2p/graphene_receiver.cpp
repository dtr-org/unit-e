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

void GrapheneReceiver::RequestAsGrapheneWhatPossible(CNode &from,
                                                     std::vector<CInv> *invs_in_out,
                                                     const CBlockIndex &last_inv_block_index,
                                                     const size_t blocks_in_flight) {
  AssertLockHeld(cs_main);

  if (!m_enabled) {
    return;
  }

  assert(invs_in_out);

  // Copying similar logic from compact block
  // This also technically means that we can request only one graphene block at a time
  if (invs_in_out->size() != 1 ||
      blocks_in_flight != 1 ||
      !last_inv_block_index.pprev->IsValid(BLOCK_VALID_CHAIN)) {
    return;
  }

  const uint256 block_hash = invs_in_out->front().hash;
  invs_in_out->erase(invs_in_out->begin());

  // Want to be consistent with global state
  assert(m_graphene_blocks_in_flight.empty());

  const auto key = std::make_pair(block_hash, from.GetId());
  m_graphene_blocks_in_flight.emplace(key, nullptr);

  GrapheneBlockRequest request(block_hash, m_txpool->GetTxCount());

  LogPrint(BCLog::NET, "Requesting graphene block %s from peer %d\n",
           request.requested_block_hash.GetHex(), from.GetId());

  PushMessage(from, NetMsgType::GETGRAPHENE, request);
}

void GrapheneReceiver::OnGrapheneBlockReceived(CNode &from,
                                               const GrapheneBlock &graphene_block) {

  const uint256 block_hash = graphene_block.header.GetHash();

  if (!m_enabled) {
    LogPrint(BCLog::NET, "Graphene block %s sent in violation of protocol, peer %d\n",
             block_hash.GetHex(), from.GetId());
    Misbehaving(from.GetId(), 100);
    return;
  }

  std::unique_ptr<GrapheneBlockReconstructor> reconstructor;

  {
    LOCK(cs_main);

    const auto key = std::make_pair(block_hash, from.GetId());
    const auto it = m_graphene_blocks_in_flight.find(key);

    if (it == m_graphene_blocks_in_flight.end()) {
      // Graphene blocks are parametrized with receiver tx pool size,
      // If we haven't requested this block => we never sent this size => we have
      // very high chance this incoming block won't decode. Don't want to spend
      // resources on it
      LogPrint(BCLog::NET, "Graphene block %s from peer %d was not requested\n",
               block_hash.GetHex(), from.GetId());
      Misbehaving(from.GetId(), 20);

      return;
    }

    if (!graphene_block.iblt.IsValid()) {
      LogPrint(BCLog::NET, "Iblt in graphene block %s is invalid, peer %d\n", block_hash.GetHex(), from.GetId());

      Misbehaving(from.GetId(), 100);
      MarkBlockNotInFlight(from, block_hash);
      return;
    }

    CValidationState val_state;
    if (!AcceptBlockHeader(graphene_block.header, val_state, Params(), nullptr)) {
      LogPrint(BCLog::NET, "Received invalid graphene block %s from peer %d\n",
               block_hash.GetHex(), from.GetId());

      int dos_score;
      if (val_state.IsInvalid(dos_score)) {
        Misbehaving(from.GetId(), dos_score);
      }

      MarkBlockNotInFlight(from, block_hash);
      return;
    }

    LogPrint(BCLog::NET, "Received graphene block %s from peer %d\n",
             block_hash.GetHex(), from.GetId());

    reconstructor = MakeUnique<GrapheneBlockReconstructor>(graphene_block, *m_txpool);

    const GrapheneDecodeState reconstructor_state = reconstructor->GetState();

    if (reconstructor_state == +GrapheneDecodeState::CANT_DECODE_IBLT) {
      LogPrint(BCLog::NET, "Unable to decode iblt in graphene block %s\n",
               block_hash.GetHex());
      RequestFallbackBlock(from, block_hash);
      return;
    } else if (reconstructor_state == +GrapheneDecodeState::NEED_MORE_TXS) {
      GrapheneTxRequest request(block_hash, reconstructor->GetMissingShortTxHashes());

      LogPrint(BCLog::NET, "Graphene block %s reconstructed, but %d transactions are missing\n",
               block_hash.GetHex(), request.missing_tx_short_hashes.size());

      PushMessage(from, NetMsgType::GETGRAPHENETX, std::move(request));
      it->second = std::move(reconstructor);
      return;
    }
  }

  ReconstructAndSubmitBlock(from, *reconstructor);
}

void GrapheneReceiver::ReconstructAndSubmitBlock(CNode &from,
                                                 const GrapheneBlockReconstructor &reconstructor) {

  const uint256 block_hash = reconstructor.GetBlockHash();
  CBlock block = reconstructor.ReconstructLTOR();

  if (CheckMerkleRoot(block)) {
    LogPrint(BCLog::NET, "Graphene block %s is valid. Submitting\n", block_hash.GetHex());

    {
      LOCK(cs_main);
      MarkBlockNotInFlight(from, block_hash);
      // This map is used ProcessNewBlock and its descendants to determine
      // source of block and to ban it if block is invalid
      mapBlockSource.emplace(block_hash, std::make_pair(from.GetId(), true));
    }

    bool new_block = false;
    ProcessNewBlock(Params(), std::make_shared<CBlock>(std::move(block)), true, &new_block);
    if (new_block) {
      from.nLastBlockTime = GetTime();
    } else {
      LOCK(cs_main);
      mapBlockSource.erase(block_hash);
    }
  } else {
    LogPrint(BCLog::NET, "Graphene block's (%s) merkle root is invalid. Requesting fallback\n",
             block_hash.GetHex());
    RequestFallbackBlock(from, block_hash);
  }
}

void GrapheneReceiver::OnGrapheneTxReceived(CNode &from, const GrapheneTx &graphene_tx) {

  const uint256 &block_hash = graphene_tx.block_hash;
  std::unique_ptr<GrapheneBlockReconstructor> reconstructor;

  {
    LOCK(cs_main);

    const auto key = std::make_pair(block_hash, from.GetId());
    const auto it = m_graphene_blocks_in_flight.find(key);

    if (it == m_graphene_blocks_in_flight.end()) {
      LogPrint(BCLog::NET, "Peer %d sent us graphene block transactions for block we weren't expecting(%s)\n",
               from.GetId(), block_hash.GetHex());
      return;
    }

    reconstructor = std::move(it->second);

    if (reconstructor == nullptr) {
      LogPrint(BCLog::NET, "Peer %d sent us graphene block transactions for block %s too early\n",
               from.GetId(), block_hash.GetHex());
      MarkBlockNotInFlight(from, block_hash);
      return;
    }

    LogPrint(BCLog::NET, "Received graphene tx for block %s, peer %d\n",
             block_hash.GetHex(), from.GetId());

    reconstructor->AddMissingTxs(graphene_tx.txs);

    if (reconstructor->GetState() != +GrapheneDecodeState::HAS_ALL_TXS) {
      LogPrint(BCLog::NET, "Can not reconstruct graphene block %s. Requesting fallback, peer %d\n",
               block_hash.GetHex(), from.GetId());
      RequestFallbackBlock(from, block_hash);

      return;
    }
  }

  ReconstructAndSubmitBlock(from, *reconstructor);
}

void GrapheneReceiver::RequestFallbackBlock(CNode &from, const uint256 &block_hash) {
  AssertLockHeld(cs_main);

  const auto key = std::make_pair(block_hash, from.GetId());
  const auto it = m_graphene_blocks_in_flight.find(key);

  assert(it != m_graphene_blocks_in_flight.end());
  m_graphene_blocks_in_flight.erase(it);

  std::vector<CInv> invs{CInv(MSG_CMPCT_BLOCK, block_hash)};
  PushMessage(from, NetMsgType::GETDATA, invs);
}

bool GrapheneReceiver::CheckMerkleRoot(const CBlock &block) {
  bool mutated;
  const uint256 merkle_root = BlockMerkleRoot(block, &mutated);

  return !mutated && block.hashMerkleRoot == merkle_root;
}

void GrapheneReceiver::OnDisconnected(const NodeId node) {
  AssertLockHeld(cs_main);

  // We expect to not have many such blocks (in fact currently one), so linear scan is acceptable
  for (auto it = m_graphene_blocks_in_flight.begin(); it != m_graphene_blocks_in_flight.end();) {
    if (it->first.second == node) {
      it = m_graphene_blocks_in_flight.erase(it);
    } else {
      ++it;
    }
  }
}

std::unique_ptr<GrapheneReceiver> GrapheneReceiver::New(Dependency<::ArgsManager> args,
                                                        Dependency<::TxPool> txpool) {
  const bool enabled = args->GetBoolArg("-graphene", true);
  return MakeUnique<GrapheneReceiver>(enabled, txpool);
}

void GrapheneReceiver::MarkBlockNotInFlight(const CNode &from, const uint256 &block_hash) {
  AssertLockHeld(cs_main);

  const auto key = std::make_pair(block_hash, from.GetId());
  const auto it = m_graphene_blocks_in_flight.find(key);

  if (it == m_graphene_blocks_in_flight.end()) {
    return;
  }

  m_graphene_blocks_in_flight.erase(it);
  MarkBlockAsReceived(block_hash);
}

void GrapheneReceiver::OnBlockReceived(NodeId node, const uint256 &block_hash) {
  AssertLockHeld(cs_main);

  const auto key = std::make_pair(block_hash, node);
  m_graphene_blocks_in_flight.erase(key);
}

}  // namespace p2p
