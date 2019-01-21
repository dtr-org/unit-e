// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/p2p_processing.h>

#include <esperanza/finalizationstate.h>
#include <snapshot/iterator.h>
#include <snapshot/snapshot_index.h>
#include <snapshot/state.h>
#include <sync.h>
#include <txdb.h>
#include <util.h>
#include <validation.h>

namespace snapshot {

inline CBlockIndex *LookupFinalizedBlockIndex(const uint256 &hash) {
  CBlockIndex *bi = LookupBlockIndex(hash);
  if (!bi) {
    return nullptr;
  }

  // todo: check that header is finalized
  // once ADR-21 is implemented
  return bi;
}

P2PState::P2PState(const Params &params) : m_params(params) {
}

bool P2PState::ProcessGetSnapshotHeader(CNode &node, CDataStream &data,
                                        const CNetMsgMaker &msg_maker) {
  uint256 snapshot_hash;
  if (!GetLatestFinalizedSnapshotHash(snapshot_hash)) {
    LogPrint(BCLog::SNAPSHOT, "%s: no finalized snapshots to return\n",
             NetMsgType::GETSNAPSHOTHEADER);
    return false;
  }

  std::unique_ptr<const Indexer> indexer = Indexer::Open(snapshot_hash);
  if (!indexer) {
    LogPrint(BCLog::SNAPSHOT, "%s: can't read snapshot %s\n",
             NetMsgType::GETSNAPSHOTHEADER,
             snapshot_hash.GetHex());
    return false;
  }

  SnapshotHeader best_snapshot;
  best_snapshot.snapshot_hash = indexer->GetMeta().snapshot_hash;
  best_snapshot.block_hash = indexer->GetMeta().block_hash;
  best_snapshot.stake_modifier = indexer->GetMeta().stake_modifier;
  best_snapshot.chain_work = indexer->GetMeta().chain_work;
  best_snapshot.total_utxo_subsets = indexer->GetMeta().total_utxo_subsets;

  LogPrint(BCLog::SNAPSHOT, "%s: return snapshot_hash=%s block_hash=%s to peer=%i\n",
           NetMsgType::GETSNAPSHOTHEADER,
           best_snapshot.snapshot_hash.GetHex(),
           best_snapshot.block_hash.GetHex(),
           node.GetId());

  g_connman->PushMessage(&node,
                         msg_maker.Make(NetMsgType::SNAPSHOTHEADER, best_snapshot));
  return true;
}

bool P2PState::ProcessSnapshotHeader(CNode &node, CDataStream &data) {
  data >> node.m_best_snapshot;
  return true;
}

bool P2PState::ProcessGetSnapshot(CNode &node, CDataStream &data,
                                  const CNetMsgMaker &msg_maker) {
  GetSnapshot get;
  data >> get;

  std::unique_ptr<Indexer> indexer = nullptr;
  for (const Checkpoint &cp : GetSnapshotCheckpoints()) {
    if (cp.snapshot_hash == get.snapshot_hash) {
      indexer = Indexer::Open(get.snapshot_hash);
      break;
    }
  }

  if (!indexer) {
    // todo: send notfound that node can act immediately
    // instead of waiting for timeout
    LogPrint(BCLog::SNAPSHOT, "%s: can't find snapshot %s\n",
             NetMsgType::GETSNAPSHOT,
             get.snapshot_hash.GetHex());
    return false;
  }

  Iterator iter(std::move(indexer));
  Snapshot snapshot;
  snapshot.snapshot_hash = iter.GetSnapshotHash();
  snapshot.utxo_subset_index = get.utxo_subset_index;

  if (!iter.GetUTXOSubsets(snapshot.utxo_subset_index, get.utxo_subset_count,
                           snapshot.utxo_subsets)) {
    LogPrint(BCLog::SNAPSHOT, "%s: requested chunk is invalid index=%i count=%i\n",
             NetMsgType::GETSNAPSHOT,
             snapshot.utxo_subset_index, get.utxo_subset_count);
    return false;
  }

  LogPrint(BCLog::SNAPSHOT, "%s: return chunk index=%i count=%i to peer=%i\n",
           NetMsgType::GETSNAPSHOT,
           snapshot.utxo_subset_index,
           snapshot.utxo_subsets.size(),
           node.GetId());

  g_connman->PushMessage(&node, msg_maker.Make(NetMsgType::SNAPSHOT, snapshot));
  return true;
}

bool P2PState::SendGetSnapshot(CNode &node, GetSnapshot &msg,
                               const CNetMsgMaker &msg_maker) {
  LogPrint(BCLog::SNAPSHOT, "send %s: peer=%i index=%i count=%i\n",
           NetMsgType::GETSNAPSHOT,
           node.GetId(), msg.utxo_subset_index, msg.utxo_subset_count);

  node.m_requested_snapshot_at = steady_clock::now();
  g_connman->PushMessage(&node, msg_maker.Make(NetMsgType::GETSNAPSHOT, msg));
  return true;
}

bool P2PState::ProcessSnapshot(CNode &node, CDataStream &data,
                               const CNetMsgMaker &msg_maker) {
  if (!IsISDEnabled()) {
    LogPrint(BCLog::SNAPSHOT, "%s: ignore the message. ISD is disabled\n",
             NetMsgType::SNAPSHOT);
    return true;
  }

  if (!LoadCandidateBlockHash().IsNull()) {
    LogPrint(BCLog::SNAPSHOT, "%s: ignore the message. Candidate is set\n",
             NetMsgType::SNAPSHOT);
    return true;
  }

  // can happen if we receive the chunk but after the timeout
  if (m_downloading_snapshot.IsNull()) {
    LogPrint(BCLog::SNAPSHOT, "%: snapshot to download is not set\n",
             NetMsgType::SNAPSHOT);
    return false;
  }

  if (node.m_best_snapshot != m_downloading_snapshot) {
    LogPrint(BCLog::SNAPSHOT, "%s: expected=%s received=%s\n",
             NetMsgType::SNAPSHOT,
             m_downloading_snapshot.snapshot_hash.GetHex(),
             node.m_best_snapshot.snapshot_hash.GetHex());
    return false;
  }

  Snapshot msg;
  data >> msg;
  if (node.m_best_snapshot.IsNull() ||
      msg.snapshot_hash != node.m_best_snapshot.snapshot_hash) {
    g_connman->Ban(node.addr, BanReasonNodeMisbehaving);
    return false;
  }

  if (msg.utxo_subset_index + msg.utxo_subsets.size() >
      node.m_best_snapshot.total_utxo_subsets) {
    LogPrint(BCLog::SNAPSHOT, "%s: invalid message index\n", NetMsgType::SNAPSHOT);
    return false;
  }

  std::unique_ptr<Indexer> indexer = Indexer::Open(msg.snapshot_hash);
  if (!indexer) {
    indexer.reset(new Indexer(msg.snapshot_hash,
                              node.m_best_snapshot.block_hash,
                              node.m_best_snapshot.stake_modifier,
                              node.m_best_snapshot.chain_work,
                              DEFAULT_INDEX_STEP, DEFAULT_INDEX_STEP_PER_FILE));
  }

  if (indexer->GetMeta().total_utxo_subsets != msg.utxo_subset_index) {
    // ask the peer the correct index
    GetSnapshot get(msg.snapshot_hash);
    get.utxo_subset_index = indexer->GetMeta().total_utxo_subsets;
    get.utxo_subset_count = MAX_UTXO_SET_COUNT;
    return SendGetSnapshot(node, get, msg_maker);
  }

  LogPrint(BCLog::SNAPSHOT, "%s: received index=%i len=%i\n",
           NetMsgType::SNAPSHOT,
           msg.utxo_subset_index, msg.utxo_subsets.size());

  if (!indexer->WriteUTXOSubsets(msg.utxo_subsets)) {
    LogPrint(BCLog::SNAPSHOT, "%s: can't write message\n", NetMsgType::SNAPSHOT);
    return false;
  }

  if (!indexer->Flush()) {
    LogPrint(BCLog::SNAPSHOT, "%s: can't update indexer\n", NetMsgType::SNAPSHOT);
    return false;
  }

  if (indexer->GetMeta().total_utxo_subsets == node.m_best_snapshot.total_utxo_subsets) {
    Iterator iterator(std::move(indexer));
    uint256 hash = iterator.CalculateHash(node.m_best_snapshot.stake_modifier,
                                          node.m_best_snapshot.chain_work);
    if (hash != msg.snapshot_hash) {
      LogPrint(BCLog::SNAPSHOT, "%s: invalid hash. has=%s got=%s\n",
               NetMsgType::SNAPSHOT,
               HexStr(hash), HexStr(msg.snapshot_hash));

      // restart the initial download from the beginning
      Indexer::Delete(msg.snapshot_hash);
      m_downloading_snapshot.SetNull();
      node.m_best_snapshot.SetNull();

      return false;
    }

    LOCK(cs_main);
    StoreCandidateBlockHash(iterator.GetBestBlockHash());
    const CBlockIndex *const bi = LookupBlockIndex(node.m_best_snapshot.block_hash);
    assert(bi);
    AddSnapshotHash(m_downloading_snapshot.snapshot_hash, bi);

    LogPrint(BCLog::SNAPSHOT, "%s: finished downloading the snapshot\n",
             NetMsgType::SNAPSHOT);
    return true;
  }

  GetSnapshot get(msg.snapshot_hash);
  get.utxo_subset_index = msg.utxo_subset_index + msg.utxo_subsets.size();
  get.utxo_subset_count = MAX_UTXO_SET_COUNT;
  return SendGetSnapshot(node, get, msg_maker);
}

void P2PState::StartInitialSnapshotDownload(CNode &node, const size_t node_index, const size_t total_nodes,
                                            const CNetMsgMaker &msg_maker) {
  if (!IsISDEnabled()) {
    return;
  }

  if (!IsInitialSnapshotDownload()) {
    return;
  }

  if (!LoadCandidateBlockHash().IsNull()) {
    // if we have the candidate snapshot, we stop
    return;
  }

  // reset best snapshot at the beginning of the loop
  // and check it the end of the iteration
  if (node_index == 0) {
    m_best_snapshot.SetNull();
  }

  if (m_first_discovery_request_at == time_point::min()) {
    m_first_discovery_request_at = steady_clock::now();
  }

  // discover the best snapshot from the peers
  if (!node.m_snapshot_discovery_sent) {
    node.m_snapshot_discovery_sent = true;

    const auto now = steady_clock::now();
    const auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - m_first_discovery_request_at);
    if (diff.count() <= m_params.discovery_timeout_sec) {
      LogPrint(BCLog::SNAPSHOT, "%s: peer=%i\n", NetMsgType::GETSNAPSHOTHEADER, node.GetId());
      g_connman->PushMessage(&node, msg_maker.Make(NetMsgType::GETSNAPSHOTHEADER));
    }
  }

  if (!IsHeadersDownloaded()) {
    return;
  }

  // start snapshot downloading

  SnapshotHeader node_best_snapshot = NodeBestSnapshot(node);
  if (!node_best_snapshot.IsNull()) {
    SetIfBestSnapshot(node_best_snapshot);

    // if the peer has the snapshot that node decided to download
    // ask for the relevant chunk from it
    if (node_best_snapshot == m_downloading_snapshot &&
        node.m_requested_snapshot_at == time_point::min()) {
      GetSnapshot msg;
      msg.snapshot_hash = m_downloading_snapshot.snapshot_hash;
      msg.utxo_subset_index = 0;
      msg.utxo_subset_count = MAX_UTXO_SET_COUNT;

      std::unique_ptr<const Indexer> indexer = Indexer::Open(node.m_best_snapshot.snapshot_hash);
      if (indexer) {
        msg.utxo_subset_index = indexer->GetMeta().total_utxo_subsets;
      }

      SendGetSnapshot(node, msg, msg_maker);
    }
  }

  // last peer processed, decide on the best snapshot
  if (node_index + 1 == total_nodes) {
    if (m_downloading_snapshot.IsNull()) {
      m_downloading_snapshot = m_best_snapshot;
    }

    // there are no nodes that can stream previously decided best snapshot
    // delete it and switch to the second best
    if (m_downloading_snapshot != m_best_snapshot) {
      Indexer::Delete(m_downloading_snapshot.snapshot_hash);
      m_downloading_snapshot = m_best_snapshot;
    }

    // if there are no peers that can provide the snapshot switch to IBD
    if (m_downloading_snapshot.IsNull()) {
      const auto now = steady_clock::now();
      const auto diff = now - m_first_discovery_request_at;
      const auto diff_sec = std::chrono::duration_cast<std::chrono::seconds>(diff);
      if (diff_sec.count() > m_params.discovery_timeout_sec) {
        LogPrint(BCLog::SNAPSHOT, "Disabling ISD and switching to IBD\n");
        DisableISDMode();
      }
      return;
    }
  }
}

void P2PState::ProcessSnapshotParentBlock(const CBlock &parent_block,
                                          std::function<void()> regular_processing) {
  if (!IsInitialSnapshotDownload()) {
    return regular_processing();
  }

  const uint256 block_hash = LoadCandidateBlockHash();
  if (block_hash.IsNull()) {
    return regular_processing();
  }

  uint256 snapshot_hash;
  CBlockIndex *snapshot_block_index;
  {
    LOCK(cs_main);

    const CBlockIndex *const block_index = LookupBlockIndex(parent_block.GetHash());
    if (!block_index || !block_index->pprev) {
      return regular_processing();
    }

    if (block_index->pprev->GetBlockHash() != block_hash) {
      return regular_processing();
    }

    // the parent block received, apply the snapshot

    uint32_t totalTxs = chainActive.Genesis()->nChainTx;

    // set one transaction for every empty header to bypass the validation
    // for the parent block
    CBlockIndex *prev = block_index->pprev;
    while (prev && prev->nHeight > 0) {
      prev->nTx = 1;
      prev->nChainTx = totalTxs + prev->nHeight;
      prev->nStatus = BLOCK_VALID_SCRIPTS;
      prev = prev->pprev;
    }

    chainActive.SetTip(block_index->pprev);
    esperanza::FinalizationState::ResetToTip(*chainActive.Tip());

    snapshot_block_index = block_index->pprev;
    assert(GetSnapshotHash(snapshot_block_index, snapshot_hash));
  }

  std::unique_ptr<Indexer> idx = Indexer::Open(snapshot_hash);
  assert(idx);
  snapshot_block_index->stake_modifier = idx->GetMeta().stake_modifier;
  snapshot_block_index->nChainWork = UintToArith256(idx->GetMeta().chain_work);

  if (!pcoinsTip->ApplySnapshot(std::move(idx))) {
    // if we can't write the snapshot, we have an issue with the DB
    // and most likely we can't recover.
    return regular_processing();
  }

  // disable block index check as at this stage we still have genesis block set
  // in setBlockIndexCandidates. It will be automatically removed after new
  // block is processed
  bool oldCheckBlockIndex = fCheckBlockIndex;
  fCheckBlockIndex = false;
  try {
    regular_processing();
  } catch (...) {
    fCheckBlockIndex = oldCheckBlockIndex;
    throw;
  }
  fCheckBlockIndex = oldCheckBlockIndex;

  // mark that blocks are pruned to pass CheckBlockIndex
  fHavePruned = true;
  pblocktree->WriteFlag("prunedblockfiles", fHavePruned);

  {
    // reduce the chance of having incompatible state after SIGKILL.
    // E.g. chainstate has a new tip but all previous headers still have nTx=0.
    LOCK(cs_main);
    FlushStateToDisk();

    // if the node was terminated after snapshot is fully downloaded but before
    // the parent block of it is processed, the blocks/index DB will be in
    // incorrect state as all snapshot headers won't be marked as "dirtyBlocks"
    // and FlushStateToDisk() function won't update them. To fix this, we
    // update the index again.
    std::vector<const CBlockIndex *> blocks;
    blocks.reserve(static_cast<uint32_t>(chainActive.Height() + 1));
    CBlockIndex *block = chainActive.Tip();
    while (block->nHeight > 0) {
      blocks.push_back(block);
      block = block->pprev;
    }
    int last_file = 0;
    pblocktree->ReadLastBlockFile(last_file);
    std::vector<std::pair<int, const CBlockFileInfo *>> file_info;
    pblocktree->WriteBatchSync(file_info, last_file, blocks);
  }

  // at this stage we are leaving ISD
  FinalizeSnapshots(snapshot_block_index);
  uint256 hash;
  assert(GetLatestFinalizedSnapshotHash(hash));
  assert(snapshot_hash == hash);
}

bool P2PState::FindNextBlocksToDownload(const NodeId node_id,
                                        std::vector<const CBlockIndex *> &blocks) {
  if (!IsISDEnabled()) {
    return false;
  }

  if (!IsInitialSnapshotDownload()) {
    return false;
  }

  const uint256 block_hash = LoadCandidateBlockHash();
  if (block_hash.IsNull()) {
    // waiting until the candidate snapshot is created
    return true;
  }

  bool sent = false;
  g_connman->ForNode(node_id, [&sent](CNode *node) mutable {
    sent = node->sentGetParentBlockForSnapshot;
    return true;
  });

  if (sent) {
    // request only once per node.
    // todo: re-request if node didn't reply within some reasonable time
    return true;
  }

  // this loop is slow but it's only performed once per node and until
  // the corresponded block for the candidate snapshot has been received
  for (const auto &pair : mapBlockIndex) {
    CBlockIndex *prev = pair.second->pprev;
    if (!prev) {
      continue;
    }

    if (prev->GetBlockHash() == block_hash) {
      blocks.emplace_back(pair.second);

      g_connman->ForNode(node_id, [](CNode *node) {
        node->sentGetParentBlockForSnapshot = true;
        return true;
      });

      return true;
    }
  }

  // we still haven't received the parent block of the snapshot
  return true;
}

SnapshotHeader P2PState::NodeBestSnapshot(CNode &node) {
  if (node.m_best_snapshot.IsNull()) {
    return {};
  }

  LOCK(cs_main);
  const CBlockIndex *const bi = LookupFinalizedBlockIndex(node.m_best_snapshot.block_hash);
  if (!bi) {
    return {};
  }

  if (node.m_requested_snapshot_at == time_point::min()) {
    return node.m_best_snapshot;
  }

  // check timeout
  const auto now = steady_clock::now();
  const auto diff = now - node.m_requested_snapshot_at;
  const auto diff_sec = std::chrono::duration_cast<std::chrono::seconds>(diff);
  if (diff_sec.count() > m_params.snapshot_chunk_timeout_sec) {
    node.m_best_snapshot.SetNull();
    return {};
  }

  return node.m_best_snapshot;
}

void P2PState::SetIfBestSnapshot(const SnapshotHeader &best_snapshot) {
  if (best_snapshot.IsNull()) {
    return;
  }

  if (m_best_snapshot.IsNull()) {
    m_best_snapshot = best_snapshot;
    return;
  }

  // if a peer has the snapshot which matches with one node downloads, mark it the best
  if (!m_downloading_snapshot.IsNull() && best_snapshot == m_downloading_snapshot) {
    m_best_snapshot = best_snapshot;
    return;
  }

  // don't switch the snapshot once it's decided to download it
  // and there are peers that can support it
  if (m_downloading_snapshot == m_best_snapshot) {
    return;
  }

  // compare heights to find the best snapshot
  LOCK(cs_main);
  const CBlockIndex *const cur_bi = LookupBlockIndex(m_best_snapshot.block_hash);
  assert(cur_bi);

  const CBlockIndex *const new_bi = LookupFinalizedBlockIndex(best_snapshot.block_hash);
  if (new_bi && new_bi->nHeight > cur_bi->nHeight) {
    m_best_snapshot = best_snapshot;
    return;
  }
}

void P2PState::DeleteUnlinkedSnapshot() {
  if (m_downloading_snapshot.IsNull()) {
    return;
  }

  // don't keep partially downloaded snapshot on disk when the node stops
  // as it will be unlinked and be never deleted
  uint256 finalized_hash;
  GetLatestFinalizedSnapshotHash(finalized_hash);
  if (m_downloading_snapshot.snapshot_hash != LoadCandidateBlockHash() &&
      m_downloading_snapshot.snapshot_hash != finalized_hash) {
    Indexer::Delete(m_downloading_snapshot.snapshot_hash);
  }
}

P2PState g_p2p_state;

void InitP2P(const Params &params) {
  g_p2p_state = P2PState(params);
}

void DeinitP2P() {
  g_p2p_state.DeleteUnlinkedSnapshot();
}

// proxy to g_p2p_state.ProcessGetSnapshotHeader
bool ProcessGetBestSnapshot(CNode &node, CDataStream &data,
                            const CNetMsgMaker &msg_maker) {
  return g_p2p_state.ProcessGetSnapshotHeader(node, data, msg_maker);
}

// proxy to g_p2p_state.ProcessSnapshotHeader
bool ProcessBestSnapshot(CNode &node, CDataStream &data) {
  return g_p2p_state.ProcessSnapshotHeader(node, data);
}

bool ProcessGetSnapshot(CNode &node, CDataStream &data,
                        const CNetMsgMaker &msg_maker) {
  return g_p2p_state.ProcessGetSnapshot(node, data, msg_maker);
}

bool ProcessSnapshot(CNode &node, CDataStream &data,
                     const CNetMsgMaker &msg_maker) {
  return g_p2p_state.ProcessSnapshot(node, data, msg_maker);
}

void StartInitialSnapshotDownload(CNode &node, const size_t node_index, const size_t total_nodes,
                                  const CNetMsgMaker &msg_maker) {
  g_p2p_state.StartInitialSnapshotDownload(node, node_index, total_nodes, msg_maker);
}

bool FindNextBlocksToDownload(const NodeId node_id,
                              std::vector<const CBlockIndex *> &blocks) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
  return g_p2p_state.FindNextBlocksToDownload(node_id, blocks);
}

void ProcessSnapshotParentBlock(const CBlock &parent_block,
                                std::function<void()> regular_processing) {
  g_p2p_state.ProcessSnapshotParentBlock(parent_block, std::move(regular_processing));
}

}  // namespace snapshot
