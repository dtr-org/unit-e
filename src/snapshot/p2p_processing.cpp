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

P2PState::P2PState(const Params &params)
    : m_params(params),
      m_first_request_at(std::chrono::steady_clock::time_point::min()),
      m_last_request_at(std::chrono::steady_clock::time_point::min()) {
}

bool P2PState::ProcessGetSnapshot(CNode *node, CDataStream &data,
                                  const CNetMsgMaker &msg_maker) {
  GetSnapshot get;
  data >> get;

  std::unique_ptr<Indexer> indexer = nullptr;
  if (get.best_block_hash.IsNull()) {  // initial request
    uint256 snapshot_hash;
    if (!GetLatestFinalizedSnapshotHash(snapshot_hash)) {
      LogPrint(BCLog::NET, "getsnapshot: no finalized snapshots\n");
      return false;
    }

    indexer = Indexer::Open(snapshot_hash);
    if (!indexer) {
      LogPrint(BCLog::NET, "getsnapshot: can't read snapshot %s\n",
               snapshot_hash.GetHex());
      return false;
    }
  } else {
    const CBlockIndex *block_index = LookupBlockIndex(get.best_block_hash);
    if (!block_index) {
      LogPrint(BCLog::NET, "snapshot: unknown block hash=%s\n",
               get.best_block_hash.GetHex());
      return false;
    }

    uint256 snapshotHash;
    if (GetFinalizedSnapshotHash(block_index, snapshotHash)) {
      indexer = Indexer::Open(snapshotHash);
    }
    if (!indexer) {
      // todo: send notfound that node can ask for newer snapshot
      // or send the newest snapshot right away
      LogPrint(BCLog::NET, "getsnapshot: can't find snapshot %s\n",
               get.best_block_hash.GetHex());
      return false;
    }
  }

  Iterator iter(std::move(indexer));
  Snapshot snapshot;
  snapshot.snapshot_hash = iter.GetSnapshotHash();
  snapshot.best_block_hash = iter.GetBestBlockHash();
  snapshot.total_utxo_subsets = iter.GetTotalUTXOSubsets();
  snapshot.utxo_subset_index = get.utxo_subset_index;

  if (!iter.GetUTXOSubsets(snapshot.utxo_subset_index, get.utxo_subset_count,
                           snapshot.utxo_subsets)) {
    LogPrint(BCLog::NET, "getsnapshot: no messages. index=%i count=%i\n",
             snapshot.utxo_subset_index, get.utxo_subset_count);
    return false;
  }

  LogPrint(BCLog::NET, "send snapshot: peer=%i index=%i count=%i\n",
           node->GetId(), snapshot.utxo_subset_index,
           snapshot.utxo_subsets.size());

  g_connman->PushMessage(node, msg_maker.Make(NetMsgType::SNAPSHOT, snapshot));
  return true;
}

bool P2PState::SendGetSnapshot(CNode *node, GetSnapshot &msg,
                               const CNetMsgMaker &msg_maker) {
  LogPrint(BCLog::NET, "send getsnapshot: peer=%i index=%i count=%i\n",
           node->GetId(), msg.utxo_subset_index, msg.utxo_subset_count);

  auto now = std::chrono::steady_clock::now();
  m_last_request_at = now;
  node->m_snapshot_requested = true;
  g_connman->PushMessage(node, msg_maker.Make(NetMsgType::GETSNAPSHOT, msg));
  return true;
}

bool P2PState::SaveSnapshotAndRequestMore(std::unique_ptr<Indexer> indexer,
                                          Snapshot &snap, CNode *node,
                                          const CNetMsgMaker &msg_maker) {
  // todo allow to accept messages not in a sequential order
  // requires to change the Indexer::WriteUTXOSubset
  if (indexer->GetMeta().total_utxo_subsets != snap.utxo_subset_index) {
    GetSnapshot get(snap.best_block_hash);
    get.utxo_subset_index = indexer->GetMeta().total_utxo_subsets;
    get.utxo_subset_count = MAX_UTXO_SET_COUNT;
    return SendGetSnapshot(node, get, msg_maker);
  }

  if (!indexer->WriteUTXOSubsets(snap.utxo_subsets)) {
    LogPrint(BCLog::NET, "snapshot: can't write message\n");
    return false;
  }

  if (!indexer->Flush()) {
    LogPrint(BCLog::NET, "snapshot: can't update indexer\n");
    return false;
  }

  if (indexer->GetMeta().total_utxo_subsets == snap.total_utxo_subsets) {
    Iterator iterator(std::move(indexer));
    uint256 hash = iterator.CalculateHash(snap.stake_modifier);
    if (hash != snap.snapshot_hash) {
      LogPrint(BCLog::NET, "snapshot: invalid hash. has=%s got=%s\n",
               HexStr(hash), HexStr(snap.snapshot_hash));

      // restart the initial download from the beginning.
      SnapshotIndex::DeleteSnapshot(snap.snapshot_hash);
      return false;
    }

    StoreCandidateBlockHash(iterator.GetBestBlockHash());

    LogPrint(BCLog::NET, "snapshot: finished downloading the snapshot\n");
    return true;
  }

  GetSnapshot get(snap.best_block_hash);
  get.utxo_subset_index = snap.utxo_subset_index + snap.utxo_subsets.size();
  get.utxo_subset_count = MAX_UTXO_SET_COUNT;
  return SendGetSnapshot(node, get, msg_maker);
}

bool P2PState::ProcessSnapshot(CNode *node, CDataStream &data,
                               const CNetMsgMaker &msg_maker) {
  if (!LoadCandidateBlockHash().IsNull()) {
    LogPrint(BCLog::NET, "snapshot: ignore the message. Candidate is set\n");
    return true;
  }

  Snapshot msg;
  data >> msg;
  LogPrint(BCLog::NET, "snapshot: received index=%i len=%i total=%i\n",
           msg.utxo_subset_index, msg.utxo_subsets.size(),
           msg.total_utxo_subsets);

  if (msg.utxo_subset_index + msg.utxo_subsets.size() >
      msg.total_utxo_subsets) {
    LogPrint(BCLog::NET, "snapshot: invalid message index\n");
    return false;
  }

  LOCK(cs_main);

  CBlockIndex *msg_block_index = LookupBlockIndex(msg.best_block_hash);
  if (!msg_block_index) {
    LogPrint(BCLog::NET, "snapshot: unknown block hash=%s\n",
             msg.best_block_hash.GetHex());
    return false;
  }

  std::unique_ptr<Indexer> indexer = nullptr;
  for (const Checkpoint &p : GetSnapshotCheckpoints()) {
    indexer = Indexer::Open(p.snapshot_hash);
    break;
  }

  if (indexer) {
    const Meta &idx_meta = indexer->GetMeta();

    CBlockIndex *cur_block_index = LookupBlockIndex(idx_meta.block_hash);
    assert(cur_block_index);

    if (cur_block_index->nHeight > msg_block_index->nHeight) {
      LogPrint(BCLog::NET, "snapshot: reject lower height. has=%i got=%i\n",
               cur_block_index->nHeight, msg_block_index->nHeight);

      // ask the peer if it has the same snapshot
      GetSnapshot get(idx_meta.block_hash);
      get.utxo_subset_index = idx_meta.total_utxo_subsets;
      get.utxo_subset_count = MAX_UTXO_SET_COUNT;
      return SendGetSnapshot(node, get, msg_maker);
    }

    if (cur_block_index->nHeight < msg_block_index->nHeight) {
      LogPrint(BCLog::NET, "snapshot: switch to new height. has=%i got=%i\n",
               cur_block_index->nHeight, msg_block_index->nHeight);

      // delete old snapshot first
      SnapshotIndex::DeleteSnapshot(idx_meta.snapshot_hash);

      AddSnapshotHash(msg.snapshot_hash, msg_block_index);
      indexer.reset(new Indexer(msg.snapshot_hash, msg.best_block_hash,
                                msg.stake_modifier,
                                DEFAULT_INDEX_STEP, DEFAULT_INDEX_STEP_PER_FILE));
    } else {
      // we don't know which snapshot is the correct one at this stage
      // so we assume the initial one.
      // todo rely on esperanza finalization. ADR-21
      if (idx_meta.snapshot_hash != msg.snapshot_hash) {
        LogPrint(BCLog::NET, "snapshot: reject snapshot hash. has=%s got=%s\n",
                 idx_meta.snapshot_hash.GetHex(), msg.snapshot_hash.GetHex());
        return false;
      }
    }

    return SaveSnapshotAndRequestMore(std::move(indexer), msg, node, msg_maker);
  }

  // always create a new snapshot if previous one can't be opened.
  // otherwise, node is stuck and can't resume initial snapshot download

  for (const Checkpoint &p : GetSnapshotCheckpoints()) {
    SnapshotIndex::DeleteSnapshot(p.snapshot_hash);
  }
  AddSnapshotHash(msg.snapshot_hash, msg_block_index);

  indexer.reset(new Indexer(msg.snapshot_hash, msg.best_block_hash,
                            msg.stake_modifier,
                            DEFAULT_INDEX_STEP, DEFAULT_INDEX_STEP_PER_FILE));
  return SaveSnapshotAndRequestMore(std::move(indexer), msg, node, msg_maker);
}

void P2PState::StartInitialSnapshotDownload(CNode *node, const CNetMsgMaker &msg_maker) {
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

  if (!IsHeadersDownloaded()) {
    return;
  }

  // discover the latest snapshot from the peers

  auto now = std::chrono::steady_clock::now();

  if (node->m_snapshot_requested) {
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_request_at);
    if (diff.count() > m_params.fast_sync_timeout_sec) {
      DisableISDMode();
    }

    return;
  }

  if (m_first_request_at == std::chrono::steady_clock::time_point::min()) {
    m_first_request_at = now;
  }
  auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - m_first_request_at);
  if (diff.count() > m_params.discovery_timeout_sec) {
    return;
  }

  // todo: add block hash locators
  GetSnapshot msg;
  msg.utxo_subset_count = MAX_UTXO_SET_COUNT;

  SendGetSnapshot(node, msg, msg_maker);
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

P2PState g_p2p_state;

void InitP2P(const Params &params) {
  g_p2p_state = P2PState(params);
}

bool ProcessGetSnapshot(CNode *node, CDataStream &data,
                        const CNetMsgMaker &msg_maker) {
  return g_p2p_state.ProcessGetSnapshot(node, data, msg_maker);
}

bool ProcessSnapshot(CNode *node, CDataStream &data,
                     const CNetMsgMaker &msg_maker) {
  return g_p2p_state.ProcessSnapshot(node, data, msg_maker);
}

void StartInitialSnapshotDownload(CNode *node, const CNetMsgMaker &msg_maker) {
  g_p2p_state.StartInitialSnapshotDownload(node, msg_maker);
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
