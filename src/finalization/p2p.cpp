// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <validation.h>
#include <finalization/p2p.h>
#include <esperanza/finalizationstate.h>

namespace finalization {
namespace p2p {

std::string Locator::ToString() const {
  return strprintf("Locator(start=%s, stop=%s)", util::to_string(start), stop.GetHex());
}

static CBlockIndex *FindMostRecentStart(CChain const &chain, Locator const &locator) {
  auto const *state = esperanza::FinalizationState::GetState();
  CBlockIndex *last = nullptr;
  for (auto const &h : locator.start) {
    auto it = mapBlockIndex.find(h);
    if (it == mapBlockIndex.end()) {
      LogPrint(BCLog::FINALIZATION, "Hash not found: %s", h.GetHex());
      return nullptr;
    }
    CBlockIndex *pindex = it->second;
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

static CBlockIndex *FindStop(Locator const &locator) {
  if (locator.stop.IsNull()) {
    return nullptr;
  }
  auto it = mapBlockIndex.find(locator.stop);
  if (it == mapBlockIndex.end()) {
    LogPrint(BCLog::FINALIZATION, "Hash %s not found, fallback to stop=0x0\n", locator.stop.GetHex());
    return nullptr;
  }
  return it->second;
}

bool ProcessGetCommits(Locator const &locator) {
  CBlockIndex *pindex = FindMostRecentStart(chainActive, locator);
  if (pindex == nullptr) {
    return error("%s: cannot find start point in locator: %s", __func__, locator.ToString());
  }
  CBlockIndex *stop = FindStop(locator);
  auto const *state = esperanza::FinalizationState::GetState();
  do {
    pindex = chainActive.Next(pindex);
    if (pindex == nullptr) {
      break;
    }
    // UNIT-E detect message overflow
    LogPrintf("Add %s (%d) to commits\n", pindex->GetBlockHash().GetHex(), pindex->nHeight);
  } while (pindex != stop && !state->IsFinalizedCheckpoint(pindex->nHeight));
  return true;
}

} // p2p
} // finalization
