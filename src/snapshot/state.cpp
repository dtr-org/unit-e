// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/state.h>

#include <snapshot/snapshot_index.h>
#include <validation.h>

namespace snapshot {

void State::StoreCandidateBlockHash(const uint256 &block_hash) {
  LOCK(m_cs_candidate_block_hash);
  m_candidate_block_hash = block_hash;
}

uint256 State::LoadCandidateBlockHash() {
  LOCK(m_cs_candidate_block_hash);
  return m_candidate_block_hash;
}

bool State::IsInitialSnapshotDownload() {
  if (m_isd_latch.load(std::memory_order_relaxed)) {
    return false;
  }

  uint256 snapshot_hash;
  if (GetLatestFinalizedSnapshotHash(snapshot_hash)) {
    LogPrint(BCLog::SNAPSHOT, "Finalized snapshot found. Set IsInitialSnapshotDownload to false\n");
    m_isd_latch.store(true, std::memory_order_relaxed);
    return false;
  }

  if (chainActive.Height() > 0) {
    // at least one full block is processed, leave ISD
    LogPrint(BCLog::SNAPSHOT, "chainActive height is not zero. Set IsInitialSnapshotDownload to false\n");
    m_isd_latch.store(true, std::memory_order_relaxed);
    return false;
  }

  return true;
}

void State::HeadersDownloaded() { m_headers_downloaded.store(true); }

bool State::IsHeadersDownloaded() { return m_headers_downloaded.load(); }

State state;

void StoreCandidateBlockHash(const uint256 &block_hash) {
  state.StoreCandidateBlockHash(block_hash);
}
uint256 LoadCandidateBlockHash() { return state.LoadCandidateBlockHash(); }
bool IsInitialSnapshotDownload() { return state.IsInitialSnapshotDownload(); }
void EnableISDMode() { state.EnableISDMode(); }
void DisableISDMode() { state.DisableISDMode(); }
bool IsISDEnabled() { return state.IsISDEnabled(); }
void HeadersDownloaded() { state.HeadersDownloaded(); }
bool IsHeadersDownloaded() { return state.IsHeadersDownloaded(); }

}  // namespace snapshot
