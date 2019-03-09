// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/finalizer_commits_handler_impl.h>

#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <esperanza/checks.h>
#include <finalization/state_processor.h>
#include <finalization/state_repository.h>
#include <net_processing.h>
#include <snapshot/p2p_processing.h>
#include <snapshot/state.h>
#include <staking/active_chain.h>
#include <validation.h>

namespace p2p {

const CBlockIndex *FinalizerCommitsHandlerImpl::FindMostRecentStart(
    const FinalizerCommitsLocator &locator) const {

  AssertLockHeld(m_active_chain->GetLock());

  const finalization::FinalizationState *const fin_state = m_repo->GetTipState();
  assert(fin_state != nullptr);

  const CBlockIndex *best_index = nullptr;

  for (const uint256 &hash : locator.start) {
    const CBlockIndex *const index = m_active_chain->GetBlockIndex(hash);
    if (index == nullptr) {
      return best_index;
    }

    // first hash in the locator.start must be finalized checkpoint
    if (best_index == nullptr) {
      if (index != m_active_chain->GetGenesis()) {
        if (!fin_state->IsFinalizedCheckpoint(index->nHeight)) {
          LogPrint(BCLog::NET, "First header in getcommits (block_hash=%s, height=%d) must be finalized checkpoint. Apprently, peer has better chain.\n",
                   index->GetBlockHash().GetHex(), index->nHeight);
          return nullptr;
        }
      }
      best_index = index;

    } else if (index->nHeight > best_index->nHeight && m_active_chain->Contains(*index)) {
      best_index = index;

    } else {
      break;
    }
  }
  return best_index;
}

const CBlockIndex *FinalizerCommitsHandlerImpl::FindStop(const FinalizerCommitsLocator &locator) const {
  AssertLockHeld(m_active_chain->GetLock());

  if (locator.stop.IsNull()) {
    return nullptr;
  }

  const CBlockIndex *const result = m_active_chain->GetBlockIndex(locator.stop);

  if (result == nullptr) {
    LogPrint(BCLog::NET, "Hash %s not found in commits locator, fallback to stop=0x0\n",
             locator.stop.GetHex());
  }
  return result;
}

HeaderAndFinalizerCommits FinalizerCommitsHandlerImpl::FindHeaderAndFinalizerCommits(
    const CBlockIndex &index, const Consensus::Params &params) const {

  HeaderAndFinalizerCommits hc(index.GetBlockHeader());
  if (index.commits.get_ptr() != nullptr) {
    hc.commits = index.commits.get();
    return hc;
  }

  if (!(index.nStatus & BLOCK_HAVE_DATA)) {
    LogPrintf("%s has no data. It's on the main chain, so this shouldn't happen. Stopping.\n",
              index.GetBlockHash().GetHex());
    assert(not("No data on the main chain"));
  }

  CBlock block;
  if (!ReadBlockFromDisk(block, &index, params)) {
    assert(not("Cannot load block from disk"));
  }

  for (const auto &tx : block.vtx) {
    if (tx->IsFinalizationTransaction()) {
      hc.commits.push_back(tx);
    }
  }

  return hc;
}

void FinalizerCommitsHandlerImpl::OnGetCommits(
    CNode &node, const FinalizerCommitsLocator &locator, const Consensus::Params &params) const {

  LOCK(m_active_chain->GetLock());

  const CBlockIndex *const start = FindMostRecentStart(locator);
  if (start == nullptr) {
    return;
  }
  const CBlockIndex *const stop = FindStop(locator);

  const finalization::FinalizationState *fin_state = m_repo->GetTipState();
  assert(fin_state != nullptr);

  const CBlockIndex *walk = start;
  assert(m_active_chain->Contains(*walk));

  FinalizerCommitsResponse response;
  do {
    walk = m_active_chain->AtHeight(walk->nHeight + 1);
    if (walk == nullptr) {
      response.status = FinalizerCommitsResponse::Status::TipReached;
      break;
    }

    // UNIT-E TODO: detect message overflow
    response.data.emplace_back(FindHeaderAndFinalizerCommits(*walk, params));

  } while (walk != stop && !fin_state->IsFinalizedCheckpoint(walk->nHeight));

  LogPrint(BCLog::NET, "Send %d headers+commits, status = %d\n",
           response.data.size(), static_cast<uint8_t>(response.status));

  PushMessage(node, NetMsgType::COMMITS, response);
}

bool FinalizerCommitsHandlerImpl::OnCommits(
    CNode &node,
    const FinalizerCommitsResponse &msg,
    const CChainParams &chainparams,
    CValidationState &err_state,
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
    }
  }

  for (const auto &d : msg.data) {

    CBlockIndex *new_index = nullptr;
    if (!AcceptBlockHeader(d.header, err_state, chainparams, &new_index)) {
      return false;
    }

    assert(new_index != nullptr);

    if (!new_index->IsValid(BLOCK_VALID_TREE)) {
      return err(100, "bad-block-index", d.header.GetHash());
    }

    new_index->ResetCommits(d.commits);
    // UNIT-E TODO: Validate commits transactions and reconstruct finalization state
  }

  // UNIT-E: Implement in two further steps: full-sync and PUSH
  switch (msg.status) {
    case FinalizerCommitsResponse::Status::StopOrFinalizationReached:
      // UNIT-E TODO: Request next bulk
      break;
    case FinalizerCommitsResponse::Status::TipReached:
      // UNIT-E TODO: Trigger fork choice if reconstructed finalization state is better than current one
      break;
    case FinalizerCommitsResponse::Status::LengthExceeded:
      // UNIT-E TODO: Just remove the assertion after LengthExceeded is implemented.
      assert(not("not implemented"));
      // Just wait the next message to come
      break;
  }
  return true;
}

}  // namespace p2p
