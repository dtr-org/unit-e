// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/p2p_processing.h>

#include <snapshot/indexer.h>
#include <snapshot/iterator.h>
#include <snapshot/messages.h>
#include <sync.h>
#include <txdb.h>
#include <util.h>
#include <validation.h>

namespace snapshot {

bool isdMode = false;
std::atomic<bool> initialHeadersDownloaded(false);

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
    uint32_t snapshotId;
    if (!pcoinsdbview->GetSnapshotId(snapshotId)) {
      LogPrint(BCLog::NET, "getsnapshot: no current snapshot\n");
      return false;
    }

    indexer = Indexer::Open(snapshotId);
    if (!indexer) {
      LogPrint(BCLog::NET, "getsnapshot: can't read snapshot %i\n", snapshotId);
      return false;
    }
  } else {
    // iterate backwards to check the most recent snapshot first
    std::vector<uint32_t> ids = pcoinsdbview->GetSnapshotIds();
    for (auto i = ids.rbegin(); i != ids.rend(); ++i) {
      indexer = Indexer::Open(*i);
      if (indexer) {
        if (indexer->GetMeta().m_bestBlockHash == get.m_bestBlockHash) {
          break;
        } else {
          indexer = nullptr;
        }
      }
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
  snapshot.m_totalUTXOSets = iter.GetTotalUTXOSets();
  snapshot.m_utxoSetIndex = get.m_utxoSetIndex;

  if (!iter.GetUTXOSets(snapshot.m_utxoSetIndex, get.m_utxoSetCount,
                        snapshot.m_utxoSets)) {
    LogPrint(BCLog::NET, "getsnapshot: no messages. index=%i count=%i\n",
             snapshot.m_utxoSetIndex, get.m_utxoSetCount);
    return false;
  }

  LogPrint(BCLog::NET, "send snapshot: peer=%i index=%i count=%i\n",
           node->GetId(), snapshot.m_utxoSetIndex, snapshot.m_utxoSets.size());

  g_connman->PushMessage(node, msgMaker.Make(NetMsgType::SNAPSHOT, snapshot));
  return true;
}

bool sendGetSnapshot(CNode *node, GetSnapshot &msg,
                     const CNetMsgMaker &msgMaker) {
  LogPrint(BCLog::NET, "send getsnapshot: peer=%i index=%i count=%i\n",
           node->GetId(), msg.m_utxoSetIndex, msg.m_utxoSetCount);

  g_connman->PushMessage(node, msgMaker.Make(NetMsgType::GETSNAPSHOT, msg));
  return true;
}

// helper function for ProcessSnapshot
bool saveSnapshotAndRequestMore(Indexer *indexer, Snapshot &snap, CNode *node,
                                const CNetMsgMaker &msgMaker) {
  // todo allow to accept messages not in a sequential order
  // requires to change the Indexer::WriteUTXOSet
  if (indexer->GetMeta().m_totalUTXOSets != snap.m_utxoSetIndex) {
    GetSnapshot get(snap.m_bestBlockHash);
    get.m_utxoSetIndex = indexer->GetMeta().m_totalUTXOSets;
    get.m_utxoSetCount = MAX_UTXO_SET_COUNT;
    return sendGetSnapshot(node, get, msgMaker);
  }

  if (!indexer->WriteUTXOSets(snap.m_utxoSets)) {
    LogPrint(BCLog::NET, "snapshot: can't write message\n");
    return false;
  }

  if (!indexer->Flush()) {
    LogPrint(BCLog::NET, "snapshot: can't update indexer\n");
    return false;
  }

  if (indexer->GetMeta().m_totalUTXOSets == snap.m_totalUTXOSets) {
    uint256 snapHash = indexer->CalcSnapshotHash();
    if (snapHash != snap.m_snapshotHash) {
      LogPrint(BCLog::NET, "snapshot: invalid hash. has=%s got=%s\n",
               HexStr(snapHash), HexStr(snap.m_snapshotHash));

      // restart the initial download from the beginning.
      pcoinsdbview->DeleteInitSnapshotId();
      return false;
    }

    if (!pcoinsdbview->SetCandidateSnapshotId(indexer->GetSnapshotId())) {
      LogPrint(BCLog::NET, "snapshot: can't set the final snapshot id\n");
      return false;
    }
    StoreCandidateBlockHash(indexer->GetMeta().m_bestBlockHash);

    LogPrint(BCLog::NET, "snapshot: finished downloading the snapshot\n");
    return true;
  }

  GetSnapshot get(snap.m_bestBlockHash);
  get.m_utxoSetIndex = snap.m_utxoSetIndex + snap.m_utxoSets.size();
  get.m_utxoSetCount = MAX_UTXO_SET_COUNT;
  return sendGetSnapshot(node, get, msgMaker);
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
           msg.m_utxoSetIndex, msg.m_utxoSets.size(), msg.m_totalUTXOSets);

  if (msg.m_utxoSetIndex + msg.m_utxoSets.size() > msg.m_totalUTXOSets) {
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
  uint32_t snapshotId;
  if (pcoinsdbview->GetInitSnapshotId(snapshotId)) {
    indexer = Indexer::Open(snapshotId);
  }

  if (indexer) {
    const snapshot::Meta &idxMeta = indexer->GetMeta();

    CBlockIndex *curBlockIndex = LookupBlockIndex(idxMeta.m_bestBlockHash);
    assert(curBlockIndex);

    if (curBlockIndex->nHeight > msgBlockIndex->nHeight) {
      LogPrint(BCLog::NET, "snapshot: reject lower height. has=%i got=%i\n",
               curBlockIndex->nHeight, msgBlockIndex->nHeight);

      // ask the peer if it has the same snapshot
      GetSnapshot get(idxMeta.m_bestBlockHash);
      get.m_utxoSetIndex = idxMeta.m_totalUTXOSets;
      get.m_utxoSetCount = MAX_UTXO_SET_COUNT;
      return sendGetSnapshot(node, get, msgMaker);
    }

    if (curBlockIndex->nHeight < msgBlockIndex->nHeight) {
      LogPrint(BCLog::NET, "snapshot: switch to new height. has=%i got=%i\n",
               curBlockIndex->nHeight, msgBlockIndex->nHeight);

      if (!pcoinsdbview->ReserveSnapshotId(snapshotId)) {
        LogPrint(BCLog::NET, "snapshot: can't reserve snapshot ID\n");
        return false;
      }

      if (!pcoinsdbview->SetInitSnapshotId(snapshotId)) {
        LogPrint(BCLog::NET, "snapshot: can't update initial snapshot ID\n");
        return false;
      }
      indexer.reset(new Indexer(snapshotId, msg.m_snapshotHash,
                                msg.m_bestBlockHash, DEFAULT_INDEX_STEP,
                                DEFAULT_INDEX_STEP_PER_FILE));
    } else {
      // we don't know which snapshot is the correct one at this stage
      // so we assume the initial one.
      // todo check what other peers are sending and switch to majority
      if (idxMeta.m_snapshotHash != msg.m_snapshotHash) {
        LogPrint(BCLog::NET, "snapshot: reject snapshot hash. has=%s got=%s\n",
                 idxMeta.m_snapshotHash.GetHex(), msg.m_snapshotHash.GetHex());
        return false;
      }
    }

    return saveSnapshotAndRequestMore(indexer.get(), msg, node, msgMaker);
  }

  // always create a new snapshot if previous one can't be opened.
  // otherwise, node is stuck and can't resume initial snapshot download
  if (!pcoinsdbview->ReserveSnapshotId(snapshotId)) {
    LogPrint(BCLog::NET, "snapshot: can't reserve snapshot ID\n");
    return false;
  }

  if (!pcoinsdbview->SetInitSnapshotId(snapshotId)) {
    LogPrint(BCLog::NET, "snapshot: can't update initial snapshot ID\n");
    return false;
  }
  indexer.reset(new Indexer(snapshotId, msg.m_snapshotHash, msg.m_bestBlockHash,
                            DEFAULT_INDEX_STEP, DEFAULT_INDEX_STEP_PER_FILE));
  return saveSnapshotAndRequestMore(indexer.get(), msg, node, msgMaker);
}

bool IsInitialSnapshotDownload() {
  static std::atomic<bool> latch(false);
  if (latch.load(std::memory_order_relaxed)) {
    return false;
  }

  uint32_t snapshotId;
  if (pcoinsdbview->GetSnapshotId(snapshotId)) {
    latch.store(true, std::memory_order_relaxed);
    return false;
  }

  return true;
}

void StartInitialSnapshotDownload(CNode *node, const CNetMsgMaker &msgMaker) {
  if (!isdMode) {
    return;
  }

  if (!IsInitialSnapshotDownload()) {
    return;
  }

  if (!LoadCandidateBlockHash().IsNull()) {
    // if we have the candidate snapshot, we stop
    return;
  }

  if (!initialHeadersDownloaded.load()) {
    return;
  }

  // discover the latest snapshot from the peers
  if (node->sentInitGetSnapshot) {
    return;
  }
  node->sentInitGetSnapshot = true;

  // todo: add block hash locators
  GetSnapshot msg;
  msg.m_utxoSetCount = MAX_UTXO_SET_COUNT;

  LogPrint(BCLog::NET, "send getsnapshot: peer=%i index=%i count=%i\n",
           node->GetId(), msg.m_utxoSetIndex, msg.m_utxoSetCount);

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
  }

  uint32_t snapshotId;
  assert(pcoinsdbview->GetCandidateSnapshotId(snapshotId));

  std::unique_ptr<snapshot::Indexer> idx = snapshot::Indexer::Open(snapshotId);
  assert(idx);
  if (!pcoinsdbview->LoadSnapshot(std::move(idx))) {
    // if we can't write the snapshot, we have an issue with the DB
    // and most likely we can't recover.
    return regularProcessing();
  }

  // update the cache
  pcoinsTip->SetBestBlock(blockHash);

  // disable block index check as at this stage we still have genesis block set
  // in setBlockIndexCandidates. It will be automatically removed after new
  // block is processed
  bool oldCheckBlockIndex = fCheckBlockIndex;
  fCheckBlockIndex = false;
  regularProcessing();
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
  assert(pcoinsdbview->SetSnapshotId(snapshotId));
}

bool FindNextBlocksToDownload(NodeId nodeId,
                              std::vector<const CBlockIndex *> &blocks)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
  if (!isdMode) {
    return false;
  }

  if (!IsInitialSnapshotDownload()) {
    return false;
  }

  uint256 blockHash = LoadCandidateBlockHash();
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
  for (const auto &iter : mapBlockIndex) {
    CBlockIndex *prev = iter.second->pprev;
    if (!prev) {
      continue;
    }

    if (prev->GetBlockHash() == blockHash) {
      blocks.emplace_back(iter.second);

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

CCriticalSection cs_candidateBlockHash;
uint256 _candidateBlockHash;

void StoreCandidateBlockHash(uint256 hash) {
  LOCK(cs_candidateBlockHash);
  _candidateBlockHash = hash;
}

uint256 LoadCandidateBlockHash() {
  LOCK(cs_candidateBlockHash);
  return _candidateBlockHash;
}

}  // namespace snapshot
