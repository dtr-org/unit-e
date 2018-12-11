// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/p2p_processing.h>

#include <snapshot/iterator.h>
#include <snapshot/snapshot_index.h>
#include <snapshot/state.h>
#include <sync.h>
#include <txdb.h>
#include <util.h>
#include <validation.h>

namespace snapshot {

// todo: remove it after merging
// https://github.com/bitcoin/bitcoin/commit/92fabcd443322dcfdf2b3477515fae79e8647d86
inline CBlockIndex *LookupBlockIndex(const uint256 &hash) {
  AssertLockHeld(cs_main);
  BlockMap::const_iterator it = mapBlockIndex.find(hash);
  return it == mapBlockIndex.end() ? nullptr : it->second;
}

inline CBlockIndex *LookupFinalizedBlockIndex(const uint256 &hash) {
  CBlockIndex *bi = LookupBlockIndex(hash);
  if (!bi) {
    return nullptr;
  }

  // todo: check that header is finalized
  // once ADR-21 is implemented
  return bi;
}

P2PState::P2PState(const Params &params)
    : m_first_discovery_request_at(time_point::min()),
      m_downloading_snapshot(),
      m_params(params),
      m_best_snapshot() {
}

bool P2PState::ProcessDiscSnapshot(CNode *node, CDataStream &data,
                                   const CNetMsgMaker &msg_maker) {
  uint256 snapshot_hash;
  if (!GetLatestFinalizedSnapshotHash(snapshot_hash)) {
    LogPrint(BCLog::NET, "%s: no finalized snapshots to return\n",
             NetMsgType::DISCSNAPSHOT);
    return false;
  }

  std::unique_ptr<Indexer> indexer = Indexer::Open(snapshot_hash);
  if (!indexer) {
    LogPrint(BCLog::NET, "%s: can't read snapshot %s\n",
             NetMsgType::DISCSNAPSHOT,
             snapshot_hash.GetHex());
    return false;
  }

  BestSnapshot best_snapshot;
  best_snapshot.snapshot_hash = indexer->GetMeta().snapshot_hash;
  best_snapshot.block_hash = indexer->GetMeta().block_hash;
  best_snapshot.stake_modifier = indexer->GetMeta().stake_modifier;
  best_snapshot.total_utxo_subsets = indexer->GetMeta().total_utxo_subsets;

  LogPrint(BCLog::NET, "%s: return snapshot_hash=%s block_hash=%s to peer=%i\n",
           NetMsgType::DISCSNAPSHOT,
           best_snapshot.snapshot_hash.GetHex(),
           best_snapshot.block_hash.GetHex(),
           node->GetId());

  g_connman->PushMessage(node,
                         msg_maker.Make(NetMsgType::BESTSNAPSHOT, best_snapshot));
  return true;
}

bool P2PState::ProcessBestSnapshot(CNode *node, CDataStream &data) {
  BestSnapshot best_snapshot;
  data >> best_snapshot;
  node->best_snapshot = best_snapshot;
  return true;
}

bool P2PState::ProcessGetSnapshot(CNode *node, CDataStream &data,
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
    LogPrint(BCLog::NET, "%s: can't find snapshot %s\n",
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
    LogPrint(BCLog::NET, "%s: requested chunk is invalid index=%i count=%i\n",
             NetMsgType::GETSNAPSHOT,
             snapshot.utxo_subset_index, get.utxo_subset_count);
    return false;
  }

  LogPrint(BCLog::NET, "%s: return chunk index=%i count=%i to peer=%i\n",
           NetMsgType::GETSNAPSHOT,
           snapshot.utxo_subset_index,
           snapshot.utxo_subsets.size(),
           node->GetId());

  g_connman->PushMessage(node, msg_maker.Make(NetMsgType::SNAPSHOT, snapshot));
  return true;
}

bool P2PState::SendGetSnapshot(CNode *node, GetSnapshot &msg,
                               const CNetMsgMaker &msg_maker) {
  LogPrint(BCLog::NET, "send %s: peer=%i index=%i count=%i\n",
           NetMsgType::GETSNAPSHOT,
           node->GetId(), msg.utxo_subset_index, msg.utxo_subset_count);

  node->requested_snapshot_at = steady_clock::now();
  g_connman->PushMessage(node, msg_maker.Make(NetMsgType::GETSNAPSHOT, msg));
  return true;
}

bool P2PState::ProcessSnapshot(CNode *node, CDataStream &data,
                               const CNetMsgMaker &msg_maker) {
  if (!IsISDEnabled()) {
    LogPrint(BCLog::NET, "%s: ignore the message. ISD is disabled\n",
             NetMsgType::SNAPSHOT);
    return true;
  }

  if (!LoadCandidateBlockHash().IsNull()) {
    LogPrint(BCLog::NET, "%s: ignore the message. Candidate is set\n",
             NetMsgType::SNAPSHOT);
    return true;
  }

  // can happen if we receive the chunk but after the timeout
  if (m_downloading_snapshot.IsNull()) {
    LogPrint(BCLog::NET, "%: snapshot to download is not set\n",
             NetMsgType::SNAPSHOT);
    return false;
  }

  if (node->best_snapshot != m_downloading_snapshot) {
    LogPrint(BCLog::NET, "%s: expected=%s received=%s\n",
             NetMsgType::SNAPSHOT,
             m_downloading_snapshot.snapshot_hash.GetHex(),
             node->best_snapshot.snapshot_hash.GetHex());
    return false;
  }

  Snapshot msg;
  data >> msg;
  if (node->best_snapshot.IsNull() ||
      msg.snapshot_hash != node->best_snapshot.snapshot_hash) {
    g_connman->Ban(node->addr, BanReasonNodeMisbehaving);
    return false;
  }

  if (msg.utxo_subset_index + msg.utxo_subsets.size() >
      node->best_snapshot.total_utxo_subsets) {
    LogPrint(BCLog::NET, "%s: invalid message index\n", NetMsgType::SNAPSHOT);
    return false;
  }

  std::unique_ptr<Indexer> indexer = Indexer::Open(msg.snapshot_hash);
  if (!indexer) {
    indexer.reset(new Indexer(msg.snapshot_hash,
                              node->best_snapshot.block_hash,
                              node->best_snapshot.stake_modifier,
                              DEFAULT_INDEX_STEP, DEFAULT_INDEX_STEP_PER_FILE));
  }

  if (indexer->GetMeta().total_utxo_subsets != msg.utxo_subset_index) {
    // ask the peer the correct index
    GetSnapshot get(msg.snapshot_hash);
    get.utxo_subset_index = indexer->GetMeta().total_utxo_subsets;
    get.utxo_subset_count = MAX_UTXO_SET_COUNT;
    return SendGetSnapshot(node, get, msg_maker);
  }

  LogPrint(BCLog::NET, "%s: received index=%i len=%i\n",
           NetMsgType::SNAPSHOT,
           msg.utxo_subset_index, msg.utxo_subsets.size());

  if (!indexer->WriteUTXOSubsets(msg.utxo_subsets)) {
    LogPrint(BCLog::NET, "%s: can't write message\n", NetMsgType::SNAPSHOT);
    return false;
  }

  if (!indexer->Flush()) {
    LogPrint(BCLog::NET, "%s: can't update indexer\n", NetMsgType::SNAPSHOT);
    return false;
  }

  if (indexer->GetMeta().total_utxo_subsets == node->best_snapshot.total_utxo_subsets) {
    Iterator iterator(std::move(indexer));
    uint256 hash = iterator.CalculateHash(node->best_snapshot.stake_modifier);
    if (hash != msg.snapshot_hash) {
      LogPrint(BCLog::NET, "%s: invalid hash. has=%s got=%s\n",
               NetMsgType::SNAPSHOT,
               HexStr(hash), HexStr(msg.snapshot_hash));

      // restart the initial download from the beginning
      Indexer::Delete(msg.snapshot_hash);
      m_downloading_snapshot.SetNull();
      node->best_snapshot.SetNull();

      return false;
    }

    LOCK(cs_main);
    StoreCandidateBlockHash(iterator.GetBestBlockHash());
    CBlockIndex *bi = LookupBlockIndex(node->best_snapshot.block_hash);
    assert(bi);
    AddSnapshotHash(m_downloading_snapshot.snapshot_hash, bi);

    LogPrint(BCLog::NET, "%s: finished downloading the snapshot\n",
             NetMsgType::SNAPSHOT);
    return true;
  }

  GetSnapshot get(msg.snapshot_hash);
  get.utxo_subset_index = msg.utxo_subset_index + msg.utxo_subsets.size();
  get.utxo_subset_count = MAX_UTXO_SET_COUNT;
  return SendGetSnapshot(node, get, msg_maker);
}

void P2PState::StartInitialSnapshotDownload(CNode *node, int node_index, int total_nodes,
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
  if (!node->snapshot_discovery_sent) {
    node->snapshot_discovery_sent = true;

    auto now = steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - m_first_discovery_request_at);
    if (diff.count() <= m_params.discovery_timeout_sec) {
      LogPrint(BCLog::NET, "%s: peer=%i\n", NetMsgType::DISCSNAPSHOT, node->GetId());
      g_connman->PushMessage(node, msg_maker.Make(NetMsgType::DISCSNAPSHOT));
    }
  }

  if (!IsHeadersDownloaded()) {
    return;
  }

  // start snapshot downloading

  BestSnapshot node_best_snapshot = NodeBestSnapshot(node);
  if (!node_best_snapshot.IsNull()) {
    SetIfBestSnapshot(node_best_snapshot);

    // if the peer has the snapshot that node decided to download
    // ask for the relevant chunk from it
    if (node_best_snapshot == m_downloading_snapshot &&
        node->requested_snapshot_at == time_point::min()) {
      GetSnapshot msg;
      msg.snapshot_hash = m_downloading_snapshot.snapshot_hash;
      msg.utxo_subset_index = 0;
      msg.utxo_subset_count = MAX_UTXO_SET_COUNT;

      std::unique_ptr<Indexer> indexer = Indexer::Open(node->best_snapshot.snapshot_hash);
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
      auto now = steady_clock::now();
      auto diff = now - m_first_discovery_request_at;
      auto diff_sec = std::chrono::duration_cast<std::chrono::seconds>(diff);
      if (diff_sec.count() > m_params.discovery_timeout_sec) {
        DisableISDMode();
      }
      return;
    }
  }
}

void P2PState::ProcessSnapshotParentBlock(CBlock *parent_block,
                                          std::function<void()> regular_processing) {
  if (!IsInitialSnapshotDownload()) {
    return regular_processing();
  }

  uint256 blockHash = LoadCandidateBlockHash();
  if (blockHash.IsNull()) {
    return regular_processing();
  }

  uint256 snapshotHash;
  CBlockIndex *snapshotBlockIndex;
  {
    LOCK(cs_main);

    CBlockIndex *blockIndex = LookupBlockIndex(parent_block->GetHash());
    if (!blockIndex || !blockIndex->pprev) {
      return regular_processing();
    }

    if (blockIndex->pprev->GetBlockHash() != blockHash) {
      return regular_processing();
    }

    // the parent block received, apply the snapshot

    uint32_t totalTxs = chainActive.Genesis()->nChainTx;

    // set one transaction for every empty header to bypass the validation
    // for the parent block
    CBlockIndex *prev = blockIndex->pprev;
    while (prev && prev->nHeight > 0) {
      prev->nTx = 1;
      prev->nChainTx = totalTxs + prev->nHeight;
      prev->nStatus = BLOCK_VALID_SCRIPTS;
      prev = prev->pprev;
    }

    chainActive.SetTip(blockIndex->pprev);

    snapshotBlockIndex = blockIndex->pprev;
    assert(GetSnapshotHash(snapshotBlockIndex, snapshotHash));
  }

  std::unique_ptr<Indexer> idx = Indexer::Open(snapshotHash);
  assert(idx);
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
    int lastFile = 0;
    pblocktree->ReadLastBlockFile(lastFile);
    std::vector<std::pair<int, const CBlockFileInfo *>> fileInfo;
    pblocktree->WriteBatchSync(fileInfo, lastFile, blocks);
  }

  // at this stage we are leaving ISD
  FinalizeSnapshots(snapshotBlockIndex);
  uint256 hash;
  assert(GetLatestFinalizedSnapshotHash(hash));
  assert(snapshotHash == hash);
}

bool P2PState::FindNextBlocksToDownload(NodeId node_id,
                                        std::vector<const CBlockIndex *> &blocks) {
  if (!IsISDEnabled()) {
    return false;
  }

  if (!IsInitialSnapshotDownload()) {
    return false;
  }

  const uint256 blockHash = LoadCandidateBlockHash();
  if (blockHash.IsNull()) {
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

    if (prev->GetBlockHash() == blockHash) {
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

BestSnapshot P2PState::NodeBestSnapshot(CNode *node) {
  if (node->best_snapshot.IsNull()) {
    return {};
  }

  LOCK(cs_main);
  CBlockIndex *bi = LookupFinalizedBlockIndex(node->best_snapshot.block_hash);
  if (!bi) {
    return {};
  }

  if (node->requested_snapshot_at == time_point::min()) {
    return node->best_snapshot;
  }

  // check timeout
  auto now = steady_clock::now();
  auto diff = now - node->requested_snapshot_at;
  auto diff_sec = std::chrono::duration_cast<std::chrono::seconds>(diff);
  if (diff_sec.count() > m_params.snapshot_chunk_timeout_sec) {
    node->best_snapshot.SetNull();
    return {};
  }

  return node->best_snapshot;
}

void P2PState::SetIfBestSnapshot(const BestSnapshot &best_snapshot) {
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
  CBlockIndex *cur_bi = LookupBlockIndex(m_best_snapshot.block_hash);
  assert(cur_bi);

  CBlockIndex *new_bi = LookupFinalizedBlockIndex(best_snapshot.block_hash);
  if (new_bi && new_bi->nHeight > cur_bi->nHeight) {
    m_best_snapshot = best_snapshot;
    return;
  }
}

P2PState g_p2p_state;

void InitP2P(const Params &params) {
  g_p2p_state = P2PState(params);
}

// proxy to g_p2p_state.ProcessDiscSnapshot
bool ProcessDiscSnapshot(CNode *node, CDataStream &data,
                         const CNetMsgMaker &msg_maker) {
  return g_p2p_state.ProcessDiscSnapshot(node, data, msg_maker);
}

// proxy to g_p2p_state.ProcessBestSnapshot
bool ProcessBestSnapshot(CNode *node, CDataStream &data) {
  return g_p2p_state.ProcessBestSnapshot(node, data);
}

bool ProcessGetSnapshot(CNode *node, CDataStream &data,
                        const CNetMsgMaker &msg_maker) {
  return g_p2p_state.ProcessGetSnapshot(node, data, msg_maker);
}

bool ProcessSnapshot(CNode *node, CDataStream &data,
                     const CNetMsgMaker &msg_maker) {
  return g_p2p_state.ProcessSnapshot(node, data, msg_maker);
}

void StartInitialSnapshotDownload(CNode *node, int node_index, int total_nodes,
                                  const CNetMsgMaker &msg_maker) {
  g_p2p_state.StartInitialSnapshotDownload(node, node_index, total_nodes, msg_maker);
}

bool FindNextBlocksToDownload(NodeId node_id,
                              std::vector<const CBlockIndex *> &blocks) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
  return g_p2p_state.FindNextBlocksToDownload(node_id, blocks);
}

void ProcessSnapshotParentBlock(CBlock *parent_block,
                                std::function<void()> regular_processing) {
  g_p2p_state.ProcessSnapshotParentBlock(parent_block, std::move(regular_processing));
}

}  // namespace snapshot
