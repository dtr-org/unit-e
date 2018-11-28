// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/p2p_processing.h>

#include <snapshot/indexer.h>
#include <snapshot/iterator.h>
#include <snapshot/messages.h>
#include <snapshot/snapshot_index.h>
#include <snapshot/state.h>
#include <sync.h>
#include <txdb.h>
#include <util.h>
#include <validation.h>

namespace snapshot {

uint16_t g_fast_sync_timeout_sec;

void InitP2P(const Params &params) {
  g_fast_sync_timeout_sec = params.fast_sync_timeout_sec;
}

// todo: remove it after merging
// https://github.com/bitcoin/bitcoin/commit/92fabcd443322dcfdf2b3477515fae79e8647d86
inline CBlockIndex *LookupBlockIndex(const uint256 &hash) {
  AssertLockHeld(cs_main);
  BlockMap::const_iterator it = mapBlockIndex.find(hash);
  return it == mapBlockIndex.end() ? nullptr : it->second;
}

bool ProcessGetSnapshot(CNode *node, CDataStream &data,
                        const CNetMsgMaker &msgMaker) {
  GetSnapshot get;
  data >> get;

  std::unique_ptr<Indexer> indexer = nullptr;
  if (get.m_bestBlockHash.IsNull()) {  // initial request
    uint256 snapshotHash;
    if (!GetLatestFinalizedSnapshotHash(snapshotHash)) {
      LogPrint(BCLog::NET, "getsnapshot: no finalized snapshots\n");
      return false;
    }

    indexer = Indexer::Open(snapshotHash);
    if (!indexer) {
      LogPrint(BCLog::NET, "getsnapshot: can't read snapshot %s\n",
               snapshotHash.GetHex());
      return false;
    }
  } else {
    const CBlockIndex *msgBlockIndex = LookupBlockIndex(get.m_bestBlockHash);
    if (!msgBlockIndex) {
      LogPrint(BCLog::NET, "snapshot: unknown block hash=%s\n",
               get.m_bestBlockHash.GetHex());
      return false;
    }

    uint256 snapshotHash;
    if (GetFinalizedSnapshotHash(msgBlockIndex, snapshotHash)) {
      indexer = Indexer::Open(snapshotHash);
    }
    if (!indexer) {
      // todo: send notfound that node can ask for newer snapshot
      // or send the newest snapshot right away
      LogPrint(BCLog::NET, "getsnapshot: can't find snapshot %s\n",
               get.m_bestBlockHash.GetHex());
      return false;
    }
  }

  Iterator iter(std::move(indexer));
  Snapshot snapshot;
  snapshot.m_snapshotHash = iter.GetSnapshotHash();
  snapshot.m_bestBlockHash = iter.GetBestBlockHash();
  snapshot.m_totalUTXOSubsets = iter.GetTotalUTXOSubsets();
  snapshot.m_utxoSubsetIndex = get.m_utxoSubsetIndex;

  if (!iter.GetUTXOSubsets(snapshot.m_utxoSubsetIndex, get.m_utxoSubsetCount,
                           snapshot.m_utxoSubsets)) {
    LogPrint(BCLog::NET, "getsnapshot: no messages. index=%i count=%i\n",
             snapshot.m_utxoSubsetIndex, get.m_utxoSubsetCount);
    return false;
  }

  LogPrint(BCLog::NET, "send snapshot: peer=%i index=%i count=%i\n",
           node->GetId(), snapshot.m_utxoSubsetIndex,
           snapshot.m_utxoSubsets.size());

  g_connman->PushMessage(node, msgMaker.Make(NetMsgType::SNAPSHOT, snapshot));
  return true;
}

bool SendGetSnapshot(CNode *node, GetSnapshot &msg,
                     const CNetMsgMaker &msgMaker) {
  LogPrint(BCLog::NET, "send getsnapshot: peer=%i index=%i count=%i\n",
           node->GetId(), msg.m_utxoSubsetIndex, msg.m_utxoSubsetCount);

  int64_t now = std::chrono::seconds(std::time(nullptr)).count();
  node->m_snapshot_requested_at = now;
  g_connman->PushMessage(node, msgMaker.Make(NetMsgType::GETSNAPSHOT, msg));
  return true;
}

// helper function for ProcessSnapshot
bool SaveSnapshotAndRequestMore(std::unique_ptr<Indexer> &&indexer,
                                Snapshot &snap, CNode *node,
                                const CNetMsgMaker &msgMaker) {
  // todo allow to accept messages not in a sequential order
  // requires to change the Indexer::WriteUTXOSubset
  if (indexer->GetMeta().m_totalUTXOSubsets != snap.m_utxoSubsetIndex) {
    GetSnapshot get(snap.m_bestBlockHash);
    get.m_utxoSubsetIndex = indexer->GetMeta().m_totalUTXOSubsets;
    get.m_utxoSubsetCount = MAX_UTXO_SET_COUNT;
    return SendGetSnapshot(node, get, msgMaker);
  }

  if (!indexer->WriteUTXOSubsets(snap.m_utxoSubsets)) {
    LogPrint(BCLog::NET, "snapshot: can't write message\n");
    return false;
  }

  if (!indexer->Flush()) {
    LogPrint(BCLog::NET, "snapshot: can't update indexer\n");
    return false;
  }

  if (indexer->GetMeta().m_totalUTXOSubsets == snap.m_totalUTXOSubsets) {
    Iterator iterator(std::move(indexer));
    uint256 snapHash = iterator.CalculateHash(snap.m_stakeModifier);
    if (snapHash != snap.m_snapshotHash) {
      LogPrint(BCLog::NET, "snapshot: invalid hash. has=%s got=%s\n",
               HexStr(snapHash), HexStr(snap.m_snapshotHash));

      // restart the initial download from the beginning.
      SnapshotIndex::DeleteSnapshot(snap.m_snapshotHash);
      return false;
    }

    StoreCandidateBlockHash(iterator.GetBestBlockHash());

    LogPrint(BCLog::NET, "snapshot: finished downloading the snapshot\n");
    return true;
  }

  GetSnapshot get(snap.m_bestBlockHash);
  get.m_utxoSubsetIndex = snap.m_utxoSubsetIndex + snap.m_utxoSubsets.size();
  get.m_utxoSubsetCount = MAX_UTXO_SET_COUNT;
  return SendGetSnapshot(node, get, msgMaker);
}

bool ProcessSnapshot(CNode *node, CDataStream &data,
                     const CNetMsgMaker &msgMaker) {
  if (!LoadCandidateBlockHash().IsNull()) {
    LogPrint(BCLog::NET, "snapshot: ignore the message. Candidate is set\n");
    return true;
  }

  Snapshot msg;
  data >> msg;
  LogPrint(BCLog::NET, "snapshot: received index=%i len=%i total=%i\n",
           msg.m_utxoSubsetIndex, msg.m_utxoSubsets.size(),
           msg.m_totalUTXOSubsets);

  if (msg.m_utxoSubsetIndex + msg.m_utxoSubsets.size() >
      msg.m_totalUTXOSubsets) {
    LogPrint(BCLog::NET, "snapshot: invalid message index\n");
    return false;
  }

  LOCK(cs_main);

  CBlockIndex *msgBlockIndex = LookupBlockIndex(msg.m_bestBlockHash);
  if (!msgBlockIndex) {
    LogPrint(BCLog::NET, "snapshot: unknown block hash=%s\n",
             msg.m_bestBlockHash.GetHex());
    return false;
  }

  std::unique_ptr<Indexer> indexer = nullptr;
  for (const Checkpoint &p : GetSnapshotCheckpoints()) {
    indexer = Indexer::Open(p.snapshotHash);
    break;
  }

  if (indexer) {
    const Meta &idxMeta = indexer->GetMeta();

    CBlockIndex *curBlockIndex = LookupBlockIndex(idxMeta.m_blockHash);
    assert(curBlockIndex);

    if (curBlockIndex->nHeight > msgBlockIndex->nHeight) {
      LogPrint(BCLog::NET, "snapshot: reject lower height. has=%i got=%i\n",
               curBlockIndex->nHeight, msgBlockIndex->nHeight);

      // ask the peer if it has the same snapshot
      GetSnapshot get(idxMeta.m_blockHash);
      get.m_utxoSubsetIndex = idxMeta.m_totalUTXOSubsets;
      get.m_utxoSubsetCount = MAX_UTXO_SET_COUNT;
      return SendGetSnapshot(node, get, msgMaker);
    }

    if (curBlockIndex->nHeight < msgBlockIndex->nHeight) {
      LogPrint(BCLog::NET, "snapshot: switch to new height. has=%i got=%i\n",
               curBlockIndex->nHeight, msgBlockIndex->nHeight);

      // delete old snapshot first
      SnapshotIndex::DeleteSnapshot(idxMeta.m_snapshotHash);

      AddSnapshotHash(msg.m_snapshotHash, msgBlockIndex);
      indexer.reset(new Indexer(msg.m_snapshotHash, msg.m_bestBlockHash,
                                msg.m_stakeModifier,
                                DEFAULT_INDEX_STEP, DEFAULT_INDEX_STEP_PER_FILE));
    } else {
      // we don't know which snapshot is the correct one at this stage
      // so we assume the initial one.
      // todo rely on esperanza finalization. ADR-21
      if (idxMeta.m_snapshotHash != msg.m_snapshotHash) {
        LogPrint(BCLog::NET, "snapshot: reject snapshot hash. has=%s got=%s\n",
                 idxMeta.m_snapshotHash.GetHex(), msg.m_snapshotHash.GetHex());
        return false;
      }
    }

    return SaveSnapshotAndRequestMore(std::move(indexer), msg, node, msgMaker);
  }

  // always create a new snapshot if previous one can't be opened.
  // otherwise, node is stuck and can't resume initial snapshot download

  for (const Checkpoint &p : GetSnapshotCheckpoints()) {
    SnapshotIndex::DeleteSnapshot(p.snapshotHash);
  }
  AddSnapshotHash(msg.m_snapshotHash, msgBlockIndex);

  indexer.reset(new Indexer(msg.m_snapshotHash, msg.m_bestBlockHash,
                            msg.m_stakeModifier,
                            DEFAULT_INDEX_STEP, DEFAULT_INDEX_STEP_PER_FILE));
  return SaveSnapshotAndRequestMore(std::move(indexer), msg, node, msgMaker);
}

void StartInitialSnapshotDownload(CNode *node, const CNetMsgMaker &msgMaker) {
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

  const int64_t now = std::chrono::seconds(std::time(nullptr)).count();

  if (node->m_snapshot_requested_at > 0) {
    if (now - node->m_snapshot_requested_at > g_fast_sync_timeout_sec) {
      // check if it was the last node that timed out then switch to IBD

      bool available = false;
      int64_t last_requested = node->m_snapshot_requested_at;
      g_connman->ForEachNode([&available, &last_requested](CNode *node) {
        if (node->m_snapshot_requested_at == 0) {
          available = true;
          return;
        }

        last_requested = std::max(node->m_snapshot_requested_at, last_requested);
      });

      bool timed_out = now - last_requested > g_fast_sync_timeout_sec;
      if (!available && timed_out) {
        DisableISDMode();
      }

      return;
    }
  }

  node->m_snapshot_requested_at = now;

  // todo: add block hash locators
  GetSnapshot msg;
  msg.m_utxoSubsetCount = MAX_UTXO_SET_COUNT;

  LogPrint(BCLog::NET, "send getsnapshot: peer=%i index=%i count=%i\n",
           node->GetId(), msg.m_utxoSubsetIndex, msg.m_utxoSubsetCount);

  g_connman->PushMessage(node, msgMaker.Make(NetMsgType::GETSNAPSHOT, msg));
}

void ProcessSnapshotParentBlock(CBlock *parentBlock,
                                std::function<void()> regularProcessing) {
  if (!IsInitialSnapshotDownload()) {
    return regularProcessing();
  }

  uint256 blockHash = LoadCandidateBlockHash();
  if (blockHash.IsNull()) {
    return regularProcessing();
  }

  uint256 snapshotHash;
  CBlockIndex *snapshotBlockIndex;
  {
    LOCK(cs_main);

    CBlockIndex *blockIndex = LookupBlockIndex(parentBlock->GetHash());
    if (!blockIndex || !blockIndex->pprev) {
      return regularProcessing();
    }

    if (blockIndex->pprev->GetBlockHash() != blockHash) {
      return regularProcessing();
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
    return regularProcessing();
  }

  // disable block index check as at this stage we still have genesis block set
  // in setBlockIndexCandidates. It will be automatically removed after new
  // block is processed
  bool oldCheckBlockIndex = fCheckBlockIndex;
  fCheckBlockIndex = false;
  try {
    regularProcessing();
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

bool FindNextBlocksToDownload(NodeId nodeId,
                              std::vector<const CBlockIndex *> &blocks)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
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
  g_connman->ForNode(nodeId, [&sent](CNode *node) mutable {
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

      g_connman->ForNode(nodeId, [](CNode *node) {
        node->sentGetParentBlockForSnapshot = true;
        return true;
      });

      return true;
    }
  }

  // we still haven't received the parent block of the snapshot
  return true;
}

}  // namespace snapshot
