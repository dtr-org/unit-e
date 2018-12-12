// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <validation.h>
#include <finalization/p2p.h>
#include <esperanza/finalizationstate.h>
#include <net.h>
#include <netmessagemaker.h>
#include <consensus/validation.h>

namespace finalization {
namespace p2p {

std::string Locator::ToString() const {
  return strprintf("Locator(start=%s, stop=%s)", util::to_string(start), stop.GetHex());
}

namespace {
static CBlockIndex const *FindMostRecentStart(CChain const &chain, Locator const &locator) {
  auto const *const state = esperanza::FinalizationState::GetState();
  CBlockIndex const *last = nullptr;
  for (uint256 const &h : locator.start) {
    auto const it = mapBlockIndex.find(h);
    if (it == mapBlockIndex.end()) {
      LogPrint(BCLog::FINALIZATION, "Block not found: %s", h.GetHex());
      return nullptr;
    }
    CBlockIndex *const pindex = it->second;
    if (last == nullptr) { // first hash in `start` must be finalized
      if (!state->IsFinalizedCheckpoint(pindex->nHeight)) {
        LogPrint(BCLog::FINALIZATION, "The first hash in locator must be finalized checkpoint: %s (%d)\n",
                 h.GetHex(), pindex->nHeight);
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

static CBlockIndex const *FindStop(Locator const &locator) {
  if (locator.stop.IsNull()) {
    return nullptr;
  }
  auto const it = mapBlockIndex.find(locator.stop);
  if (it == mapBlockIndex.end()) {
    LogPrint(BCLog::FINALIZATION, "Hash %s not found, fallback to stop=0x0\n", locator.stop.GetHex());
    return nullptr;
  }
  return it->second;
}

static HeaderAndCommits FindHeaderAndCommits(CBlockIndex const *pindex, Consensus::Params const &params) {
  if (!(pindex->nStatus & BLOCK_HAVE_DATA)) {
    LogPrintf("%s has no data. It's on the main chain, so this shouldn't happen. Stopping.\n",
              pindex->GetBlockHash().GetHex());
    assert(not("No data on the main chain"));
  }
  HeaderAndCommits hc(pindex->GetBlockHeader());
  std::shared_ptr<CBlock> const pblock = std::make_shared<CBlock>();
  if (!ReadBlockFromDisk(*pblock, pindex, params)) {
    assert(not("Cannot load block from the disk"));
  }
  for (auto const &tx : pblock->vtx) {
    if (tx->IsCommit()) {
      hc.commits.push_back(tx);
    }
  }
  return hc;
}
} // namespace

bool ProcessGetCommits(CNode *node, Locator const &locator, CNetMsgMaker const &msgMaker,
                       CChainParams const &chainparams) {
  CBlockIndex const *pindex = FindMostRecentStart(chainActive, locator);
  if (pindex == nullptr) {
    return error("%s: cannot find start point in locator: %s", __func__, locator.ToString());
  }
  CBlockIndex const *const stop = FindStop(locator);
  auto const *const state = esperanza::FinalizationState::GetState();
  CommitsResponse r;
  do {
    pindex = chainActive.Next(pindex);
    if (pindex == nullptr) {
      r.status = CommitsResponse::Status::TipReached;
      break;
    }
    // UNIT-E detect message overflow
    r.data.emplace_back(FindHeaderAndCommits(pindex, chainparams.GetConsensus()));
  } while (pindex != stop && !state->IsFinalizedCheckpoint(pindex->nHeight));
  LogPrint(BCLog::NET, "Send %d headers+commits, status = %d\n",
           r.data.size(), static_cast<uint8_t>(r.status));
  g_connman->PushMessage(node, msgMaker.Make(NetMsgType::COMMITS, r));
  return true;
}

bool ProcessNewCommits(CommitsResponse const &msg, CChainParams const &chainparams) {
  CValidationState state;
  for (auto const &d : msg.data) {
    // UNIT-E: Check commits merkle root after it is added
    for (auto const &c : d.commits) {
      if (!c->IsCommit()) {
        return error("Found non-commit transaction, stop process commits");
      }
    }
  }
  for (auto const &d : msg.data) {
    CBlockIndex *pindex = nullptr;
    if (!AcceptBlockHeader(d.header, state, chainparams, &pindex)) {
      return false;
    }
    assert(pindex != nullptr);
    if (!pindex->IsValid(BLOCK_VALID_TREE)) {
      return error("%s is invalid, stop process commits", pindex->GetBlockHash().GetHex());
    }
    pindex->ResetCommits(d.commits);
    // UNIT-E: Validate commits transactions and reconstruct finalization state
  }
  // UNIT-E: Implement in two further steps: full-sync and PUSH
  switch (msg.status) {
  case CommitsResponse::StopOrFinReached:
    // UNIT-E: Request next bulk
    break;
  case CommitsResponse::TipReached:
    // UNIT-E: Trigger fork choice if reconstructed finalization state is better than current one
    break;
  case CommitsResponse::LengthExceeded:
    // UNIT-E: Wait the next message
    break;
  }
  return true;
}


} // p2p
} // finalization
