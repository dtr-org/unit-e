// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/finalizer_commits_handler_impl.h>

#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <esperanza/checks.h>
#include <finalization/state_processor.h>
#include <finalization/state_repository.h>
#include <finalization/vote_recorder.h>
#include <net_processing.h>
#include <snapshot/p2p_processing.h>
#include <snapshot/state.h>
#include <staking/active_chain.h>
#include <validation.h>

namespace p2p {

namespace {
bool FinalizerCommitsEqual(
    const std::vector<CTransactionRef> &a, const std::vector<CTransactionRef> &b) {

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

const CBlockIndex &FinalizerCommitsHandlerImpl::GetCheckpointIndex(
    const uint32_t epoch, const finalization::FinalizationState &fin_state) const {

  AssertLockHeld(m_active_chain->GetLock());

  const uint32_t height = fin_state.GetEpochCheckpointHeight(epoch);

  const CBlockIndex *index = m_active_chain->AtHeight(height);
  assert(index != nullptr);

  return *index;
}

const CBlockIndex &FinalizerCommitsHandlerImpl::FindLastFinalizedCheckpoint(
    const finalization::FinalizationState &fin_state) const {

  AssertLockHeld(m_active_chain->GetLock());

  const uint32_t epoch = fin_state.GetLastFinalizedEpoch();

  // Workaround 0th epoch finalization. #570
  if (epoch == 0) {
    return *m_active_chain->GetGenesis();
  }

  return GetCheckpointIndex(epoch, fin_state);
}

//! With no finalization we can reach limit of the network message just by constructing locator.
//! - 256 because of locator.start.
//! - 1024 for message header overhead, vector encoding overhead, and just in case.
constexpr size_t LOCATOR_START_LIMIT = MAX_PROTOCOL_MESSAGE_LENGTH / 256 - 256 - 1024;

FinalizerCommitsLocator FinalizerCommitsHandlerImpl::GetFinalizerCommitsLocator(
    const CBlockIndex &start, const CBlockIndex *const stop) const {

  LOCK(m_active_chain->GetLock());

  FinalizerCommitsLocator locator;

  if (stop != nullptr) {
    locator.stop = stop->GetBlockHash();
  }

  const finalization::FinalizationState *const fin_state = m_repo->GetTipState();
  assert(fin_state != nullptr);

  const CBlockIndex *const fork_origin = m_active_chain->FindForkOrigin(start);
  const CBlockIndex &last_finalized_index = FindLastFinalizedCheckpoint(*fin_state);

  assert(fork_origin != nullptr);

  const CBlockIndex *start_ptr = &start;
  if (fork_origin->nHeight < last_finalized_index.nHeight) {
    start_ptr = m_active_chain->GetTip();
  }

  assert(start_ptr != nullptr);

  const uint32_t start_epoch = fin_state->GetEpoch(*start_ptr);

  const blockchain::Height last_checkpoint_height =
      (start_epoch > 0
           ? fin_state->GetEpochCheckpointHeight(start_epoch - 1)
           : 0);

  if (static_cast<blockchain::Height>(start_ptr->nHeight) > last_checkpoint_height && start_ptr != &last_finalized_index) {
    locator.start.push_back(start_ptr->GetBlockHash());
  }

  const CBlockIndex *walk = start_ptr;
  for (blockchain::Height height = last_checkpoint_height;
       height > static_cast<blockchain::Height>(last_finalized_index.nHeight) && locator.start.size() < LOCATOR_START_LIMIT;
       height -= std::min(height, fin_state->GetEpochLength())) {

    walk = walk->GetAncestor(height);
    assert(walk != nullptr);

    locator.start.push_back(walk->GetBlockHash());
  }

  locator.start.push_back(last_finalized_index.GetBlockHash());

  std::reverse(locator.start.begin(), locator.start.end());

  return locator;
}

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
          LogPrint(BCLog::NET, "First header in getcommits (block_hash=%s height=%d) must be finalized checkpoint. Apparently, peer has better chain.\n",
                   index->GetBlockHash().GetHex(), index->nHeight);
          return nullptr;
        }
      }
      best_index = index;

    } else if (index->nHeight > best_index->nHeight) {
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

boost::optional<HeaderAndFinalizerCommits> FinalizerCommitsHandlerImpl::FindHeaderAndFinalizerCommits(
    const CBlockIndex &index, const Consensus::Params &params) const {

  HeaderAndFinalizerCommits hc(index.GetBlockHeader());
  if (index.commits) {
    hc.commits = *index.commits;
    return hc;
  }

  if (!(index.nStatus & BLOCK_HAVE_DATA)) {
    return boost::none;
  }

  CBlock block;
  if (!ReadBlockFromDisk(block, &index, params)) {
    assert(not("Cannot load block from the disk"));
  }

  for (const auto &tx : block.vtx) {
    if (tx->IsFinalizerCommit()) {
      hc.commits.push_back(tx);
    }
  }

  index.commits = hc.commits;
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
    walk = m_active_chain->GetNext(*walk);
    if (walk == nullptr) {
      response.status = FinalizerCommitsResponse::Status::TipReached;
      break;
    }

    const boost::optional<HeaderAndFinalizerCommits> header_and_commits =
        FindHeaderAndFinalizerCommits(*walk, params);
    if (!header_and_commits) {
      continue;
    }

    // In case of long unjustified dynasty we can reach the message length limit.
    // To prevent this, compute message length on every iteration and set status=
    // LengthExceeded once limit reached.
    FinalizerCommitsResponse response_copy = response;

    response_copy.data.emplace_back(*header_and_commits);

    const size_t response_size = GetSerializeSize(response_copy, SER_NETWORK, PROTOCOL_VERSION);
    if (response_size >= MAX_PROTOCOL_MESSAGE_LENGTH) {
      response.status = FinalizerCommitsResponse::Status::LengthExceeded;
      LogPrint(BCLog::NET, "Send %d headers+commits, status = %d\n",
               response.data.size(), static_cast<uint8_t>(response.status));
      PushMessage(node, NetMsgType::COMMITS, std::move(response));
      // Stakoverflow driven development is here!
      // https://stackoverflow.com/questions/7027523
      response = FinalizerCommitsResponse();
    } else {
      response = std::move(response_copy);
    }

  } while (walk != stop && !fin_state->IsFinalizedCheckpoint(walk->nHeight));

  if (response.data.empty()) {
    return;
  }

  LogPrint(BCLog::NET, "Send %d headers+commits, status = %d\n",
           response.data.size(), static_cast<uint8_t>(response.status));

  PushMessage(node, NetMsgType::COMMITS, std::move(response));
}

bool FinalizerCommitsHandlerImpl::IsSameFork(
    const CBlockIndex *head, const CBlockIndex *test, const CBlockIndex *&prev) {

  if (test->pprev == prev) {
    prev = test;
    return true;
  }

  if (head->GetAncestor(test->nHeight) != test) {
    return false;
  }

  prev = test;
  return true;
}

bool FinalizerCommitsHandlerImpl::OnCommits(
    CNode &node,
    const FinalizerCommitsResponse &msg,
    const CChainParams &chainparams,
    CValidationState &err_state,
    uint256 *failed_block_out) {

  const auto err = [&](int dos, const std::string &str, const uint256 &block) {
    if (failed_block_out != nullptr) {
      *failed_block_out = block;
    }
    return err_state.DoS(dos, false, REJECT_INVALID, str);
  };

  if (msg.data.empty()) {
    return err_state.DoS(100, false, REJECT_INVALID, "bad-commits-empty");
  }

  for (const HeaderAndFinalizerCommits &d : msg.data) {
    const uint256 commits_merkle_root = ComputeMerkleRoot(d.commits);
    if (commits_merkle_root != d.header.hash_finalizer_commits_merkle_root) {
      return err(100, "bad-finalizer-commits-merkle-root", d.header.GetHash());
    }
    for (const auto &c : d.commits) {
      if (!c->IsFinalizerCommit()) {
        return err(100, "bad-non-commit", d.header.GetHash());
      }
      // Make simplest checks which doesn't depend on the context.
      if (!(CheckTransaction(*c, err_state) && esperanza::CheckFinalizerCommit(*c, err_state))) {
        return false;
      }
    }
  }

  std::list<const CBlockIndex *> to_append;

  const CBlockIndex *last_index = nullptr;
  {
    LOCK(m_active_chain->GetLock());

    for (const HeaderAndFinalizerCommits &d : msg.data) {

      CBlockIndex *new_index = nullptr;
      if (!AcceptBlockHeader(d.header, err_state, chainparams, &new_index)) {
        return false;
      }

      assert(new_index != nullptr);

      if (last_index != nullptr && new_index->pprev != last_index) {
        return err(100, "bad-block-ordering", d.header.GetHash());
      }

      // UNIT-E TODO: Store finalizer transactions somewhere.
      // We cannot perform ContextualCheck now as it relies on GetTransaction which effectively
      // loads prev transaction from the disk. During commits exchange we do not have such data
      // on the disk.
      // So, now just record the votes. ContextualCheck would be performed later after block
      // arrives.

      for (const auto &c : d.commits) {
        if (c->IsVote()) {
          if (!finalization::RecordVote(*c, err_state)) {
            return false;
          }
        }
      }

      if (new_index->commits) {
        if (!FinalizerCommitsEqual(*new_index->commits, d.commits)) {
          // This should be almost impossible with commits merkle root validation, check it just in case
          assert(not("not implemented"));
        }
      } else {
        new_index->commits = d.commits;
      }

      if (!m_proc->ProcessNewCommits(*new_index, d.commits)) {
        return err(10, "bad-commits", d.header.GetHash());
      }

      to_append.emplace_back(new_index);

      last_index = new_index;
    }
  }

  // At this point we must either:
  // * process commits and find last_index;
  // * return if one of the commits is broken;
  // * return if we receive empty commits set.
  assert(last_index != nullptr);

  UpdateBlockAvailability(node.GetId(), last_index->GetBlockHash());

  blockchain::Height download_until = 0;

  const finalization::FinalizationState *tip_state = m_repo->GetTipState();
  const finalization::FinalizationState *index_state = m_repo->Find(*last_index);
  assert(tip_state != nullptr);
  assert(index_state != nullptr);

  const uint32_t index_epoch = index_state->GetLastFinalizedEpoch();
  const uint32_t tip_epoch = tip_state->GetLastFinalizedEpoch();

  if (index_epoch > tip_epoch) {
    download_until = index_state->GetEpochCheckpointHeight(index_epoch + 1);
    LogPrint(BCLog::NET, "Commits sync reached finalization at epoch=%d, mark blocks up to height %d to download\n",
             index_epoch, download_until);
  }

  switch (msg.status) {
    case FinalizerCommitsResponse::Status::StopOrFinalizationReached:
      LogPrint(BCLog::NET, "Request next bunch of headers+commits, height=%d\n", last_index->nHeight);
      PushMessage(node, NetMsgType::GETCOMMITS, GetFinalizerCommitsLocator(*last_index, nullptr));
      break;

    case FinalizerCommitsResponse::Status::TipReached:
      LogPrint(BCLog::NET, "Commits sync finished after processing header=%s, height=%d\n",
               last_index->GetBlockHash().GetHex(), last_index->nHeight);
      download_until = last_index->nHeight;
      break;

    case FinalizerCommitsResponse::Status::LengthExceeded:
      // Just wait the next message to come
      break;
  }

  LOCK(cs);

  auto &wait_list = m_wait_list[node.GetId()];
  wait_list.insert(to_append.begin(), to_append.end());

  if (download_until > 0) {

    auto &to_download = m_blocks_to_download[node.GetId()];
    const CBlockIndex *prev = nullptr;

    for (auto it = wait_list.begin(); it != wait_list.end();) {
      const CBlockIndex *index = *it;

      assert(index != nullptr);

      if (static_cast<blockchain::Height>(index->nHeight) > download_until) {
        break;
      }

      if (IsSameFork(last_index, index, prev)) {
        if (!(index->nStatus & BLOCK_HAVE_DATA)) {
          to_download.emplace_back(index);
        }
        it = wait_list.erase(it);
      } else {
        ++it;
      }
    }
  }

  return true;
}

void FinalizerCommitsHandlerImpl::OnDisconnect(const NodeId nodeid) {
  LOCK(cs);
  m_wait_list.erase(nodeid);
  m_blocks_to_download.erase(nodeid);
}

bool FinalizerCommitsHandlerImpl::FindNextBlocksToDownload(
    const NodeId nodeid, const size_t count, std::vector<const CBlockIndex *> &blocks_out) {

  LOCK(cs);

  if (count == 0) {
    return false;
  }

  auto const it = m_blocks_to_download.find(nodeid);
  if (it == m_blocks_to_download.end()) {
    return false;
  }

  std::list<const CBlockIndex *> &to_download = it->second;
  if (to_download.empty()) {
    return false;
  }

  size_t added_count = 0;

  while (added_count < count && !to_download.empty()) {
    const CBlockIndex *index = to_download.front();
    if (!(index->nStatus & BLOCK_HAVE_DATA)) {
      blocks_out.emplace_back(index);
      ++added_count;
    }
    to_download.pop_front();
  }

  if (added_count > 0) {
    LogPrint(BCLog::NET, "Commits full sync asked for %d blocks to download\n", added_count);
    return true;
  }
  return false;
};

}  // namespace p2p
