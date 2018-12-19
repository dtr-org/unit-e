// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <esperanza/checks.h>
#include <esperanza/finalizationstate.h>
#include <finalization/cache.h>
#include <finalization/p2p.h>
#include <net.h>
#include <net_processing.h>
#include <netmessagemaker.h>
#include <snapshot/p2p_processing.h>
#include <snapshot/state.h>
#include <validation.h>

namespace finalization {
namespace p2p {

namespace {
std::map<uint256, NodeId> requested_blocks;

void MarkRequested(const uint256 &hash, NodeId id, const CBlockIndex *index) {
  requested_blocks.emplace(hash, id);
  WIPMarkBlockAsInFlight(id, hash, index);
}

NodeId MarkReceived(const uint256 &hash) {
  const auto it = requested_blocks.find(hash);
  if (it == requested_blocks.end()) {
    return -1;
  }
  const auto id = it->second;
  requested_blocks.erase(it);
  return id;
}
}  // namespace

std::string CommitsLocator::ToString() const {
  return strprintf("Locator(start=%s, stop=%s)", util::to_string(start), stop.GetHex());
}

namespace {
CBlockIndex *FindMostRecentStart(const CChain &chain, const CommitsLocator &locator, bool *ok = nullptr) {
  const auto *const state = esperanza::FinalizationState::GetState();
  CBlockIndex *last = nullptr;
  for (const uint256 &h : locator.start) {
    const auto it = mapBlockIndex.find(h);
    if (it == mapBlockIndex.end()) {
      if (last == nullptr) {
        LogPrint(BCLog::FINALIZATION, "Block not found: %s\n", h.GetHex());
      }
      return last;
    }
    CBlockIndex *const pindex = it->second;
    if (last == nullptr) {  // first hash in `start` must be finalized
      if (!state->IsFinalizedCheckpoint(pindex->nHeight) && pindex != chain.Genesis()) {
        LogPrint(BCLog::FINALIZATION, "The first hash in locator must be finalized checkpoint: %s (%d)\n",
                 h.GetHex(), pindex->nHeight);
        if (ok != nullptr) {
          *ok = false;
        }
        return nullptr;
      }
      last = pindex;
      assert(chain.Contains(pindex));
    } else if (pindex->nHeight > last->nHeight && chain.Contains(pindex)) {
      last = pindex;
    } else {
      break;
    }
  }
  return last;
}

const CBlockIndex *FindStop(const CommitsLocator &locator) {
  if (locator.stop.IsNull()) {
    return nullptr;
  }
  const auto it = mapBlockIndex.find(locator.stop);
  if (it == mapBlockIndex.end()) {
    LogPrint(BCLog::FINALIZATION, "Hash %s not found, fallback to stop=0x0\n", locator.stop.GetHex());
    return nullptr;
  }
  return it->second;
}

HeaderAndCommits FindHeaderAndCommits(CBlockIndex *pindex, const Consensus::Params &params) {
  HeaderAndCommits hc(pindex->GetBlockHeader());
  if (pindex->HasCommits()) {
    hc.commits = pindex->commits.get();
    return hc;
  }
  if (!(pindex->nStatus & BLOCK_HAVE_DATA)) {
    LogPrint(BCLog::FINALIZATION, "%s has no data. It's on the main chain, so this shouldn't happen. Stopping.\n",
             pindex->GetBlockHash().GetHex());
    assert(not("No data on the main chain"));
  }
  const std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
  if (!ReadBlockFromDisk(*pblock, pindex, params)) {
    assert(not("Cannot load block from the disk"));
  }
  for (const auto &tx : pblock->vtx) {
    if (tx->IsFinalizationTransaction()) {
      hc.commits.push_back(tx);
    }
  }
  pindex->ResetCommits(hc.commits);
  return hc;
}
}  // namespace

bool ProcessGetCommits(CNode *node, const CommitsLocator &locator, const CNetMsgMaker &msgMaker,
                       const CChainParams &chainparams) {
  bool ok = true;
  CBlockIndex *pindex = FindMostRecentStart(chainActive, locator, &ok);
  if (pindex == nullptr) {
    if (ok) {
      return true;
    } else {
      return error("%s: cannot find start point in locator: %s", __func__, locator.ToString());
    }
  }
  const CBlockIndex *const stop = FindStop(locator);
  CommitsResponse r;
  do {
    pindex = chainActive.Next(pindex);
    if (pindex == nullptr) {
      r.status = CommitsResponse::Status::TipReached;
      break;
    }
    // UNIT-E TODO: detect message overflow
    r.data.emplace_back(FindHeaderAndCommits(pindex, chainparams.GetConsensus()));
  } while (pindex != stop && !esperanza::FinalizationState::GetState()->IsFinalizedCheckpoint(pindex->nHeight));
  LogPrint(BCLog::NET, "Send %d headers+commits, status = %d\n",
           r.data.size(), static_cast<uint8_t>(r.status));
  g_connman->PushMessage(node, msgMaker.Make(NetMsgType::COMMITS, r));
  return true;
}

namespace {
bool CompareCommits(const std::vector<CTransactionRef> &a, const std::vector<CTransactionRef> &b) {
  if (a.size() != b.size()) {
    return false;
  }
  if (a == b) {
    return true;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i]->GetHash() != b[i]->GetHash()) {
      return false;
    }
  }
  return true;
}
}  // namespace

bool ProcessNewCommits(CNode *node, const CommitsResponse &msg, const CNetMsgMaker &msgMaker,
                       const CChainParams &chainparams, CValidationState &err_state,
                       uint256 *failed_block_out) {
  const auto err = [&](int code, const std::string &str, const uint256 &block) {
    if (failed_block_out != nullptr) {
      *failed_block_out = block;
    }
    return err_state.DoS(code, false, REJECT_INVALID, str);
  };
  for (const auto &d : msg.data) {
    // UNIT-E TODO: Check commits merkle root after it is added
    for (const auto &c : d.commits) {
      if (!c->IsFinalizationTransaction()) {
        return err(100, "bad-non-commit", d.header.GetHash());
      }
      if (not(CheckTransaction(*c, err_state) && esperanza::CheckFinalizationTx(*c, err_state))) {
        return false;
      }
    }
  }
  std::vector<const CBlockIndex *> to_download;
  const bool is_in_snapshot = snapshot::FindNextBlocksToDownload(node->GetId(), to_download);
  if (!is_in_snapshot) {
    assert(to_download.empty());
  }
  CBlockIndex *last_index = nullptr;
  for (const auto &d : msg.data) {
    // UNIT-E TODO: we should trigger error if message contains headers from two dynasties
    CBlockIndex *new_index = nullptr;
    if (!AcceptBlockHeader(d.header, err_state, chainparams, &new_index)) {
      return false;
    }
    assert(new_index != nullptr);
    if (!new_index->IsValid(BLOCK_VALID_TREE)) {
      return err(100, "bad-block-index", d.header.GetHash());
    }
    if (last_index != nullptr && new_index->pprev != last_index) {
      return err(100, "bad-block-ordering", d.header.GetHash());
    }
    if (new_index->HasCommits()) {
      if (!CompareCommits(new_index->GetCommits(), d.commits)) {
        // This should be almost impossible with commits merkle root validation, check it just in case
        assert(not("not implemented"));
      }
    } else {
      new_index->ResetCommits(d.commits);
    }

    if (!finalization::cache::ProcessNewCommits(*new_index, d.commits)) {
      return err(10, "bad-commits", d.header.GetHash());
    }

    // UNIT-E TODO: Move getdata to net_processing.cpp, use inflight logic.
    if (!is_in_snapshot && !(new_index->nStatus & BLOCK_HAVE_DATA)) {
      to_download.emplace_back(new_index);
    }
    last_index = new_index;
  }

  if (last_index != nullptr) {
    UpdateBlockAvailability(node->GetId(), last_index->GetBlockHash());
  }

  std::vector<CInv> getdata;
  for (const auto &block_index : to_download) {
    MarkRequested(block_index->GetBlockHash(), node->GetId(), block_index);
    getdata.emplace_back(MSG_BLOCK | MSG_WITNESS_FLAG, block_index->GetBlockHash());
  }

  if (is_in_snapshot) {
    if (msg.status == CommitsResponse::Status::TipReached) {
      snapshot::HeadersDownloaded();
    } else {
      g_connman->PushMessage(node, msgMaker.Make(NetMsgType::GETCOMMITS, finalization::p2p::GetCommitsLocator(last_index, nullptr)));
    }
  }

  if (getdata.size() > 0) {
    g_connman->PushMessage(node, msgMaker.Make(NetMsgType::GETDATA, getdata));
  }

  return true;
}

namespace {
// Returns CBlockIndex of checkpoint (last block) in epoch
const CBlockIndex *GetCheckpointIndex(uint32_t epoch,
                                      const CChain &chain,
                                      const esperanza::FinalizationState &fin_state) {
  const auto h = fin_state.GetEpochStartHeight(epoch + 1) - 1;
  const auto *index = chain[h];
  assert(index != nullptr);
  return index;
}

const CBlockIndex *FindLastFinalizedCheckpoint(const CChain &chain,
                                               const esperanza::FinalizationState &fin_state) {
  const auto e = fin_state.GetLastFinalizedEpoch();
  if (e == 0) {
    return chain.Genesis();
  }
  return GetCheckpointIndex(e, chain, fin_state);
}
}  // namespace

CommitsLocator GetCommitsLocator(const CBlockIndex *const start,
                                 const CBlockIndex *const stop) {
  CommitsLocator locator;
  if (stop != nullptr) {
    locator.stop = stop->GetBlockHash();
  }
  const auto fin_state = esperanza::FinalizationState::GetState(chainActive.Tip());
  assert(fin_state != nullptr);

  const CBlockIndex *finalized = FindLastFinalizedCheckpoint(chainActive, *fin_state);
  const CBlockIndex *last_index = nullptr;

  if (start == nullptr) {
    last_index = chainActive.Tip();
  } else {
    last_index = start;
    if (last_index->nHeight > finalized->nHeight) {
      locator.start.push_back(last_index->GetBlockHash());
    }
  }

  assert(last_index != nullptr);
  for (const auto *index = last_index; index != nullptr && index->nHeight > finalized->nHeight; index = index->pprev) {
    if (fin_state->IsCheckpoint(index->nHeight)) {
      locator.start.push_back(index->GetBlockHash());
    }
  }

  locator.start.push_back(finalized->GetBlockHash());
  std::reverse(locator.start.begin(), locator.start.end());
  return locator;
}

void OnBlock(const uint256 &block_hash) {
  const auto node_id = MarkReceived(block_hash);
  if (node_id == -1) {
    return;
  }
  // This function must be called after accepting the block
  // so it must be in the mapBlockIndex.
  const auto *const index = LookupBlockIndex(block_hash);
  assert(index != nullptr);
  if (esperanza::IsCheckpoint(index->nHeight)) {
    LogPrint(BCLog::FINALIZATION, "request next commits after %s\n", block_hash.GetHex());
    g_connman->ForNode(node_id, [index](CNode *node) {
      const CNetMsgMaker msgMaker(node->GetSendVersion());
      g_connman->PushMessage(node, msgMaker.Make(NetMsgType::GETCOMMITS, finalization::p2p::GetCommitsLocator(index, nullptr)));
      return true;
    });
  }
}

}  // namespace p2p
}  // namespace finalization
