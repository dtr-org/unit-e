// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/state.h>

#include <snapshot/snapshot_index.h>
#include <validation.h>

namespace snapshot {

void State::StoreCandidateBlockHash(const uint256 &hash) {
  LOCK(cs_candidateBlockHash);
  m_candidateHash = hash;
}

uint256 State::LoadCandidateBlockHash() {
  LOCK(cs_candidateBlockHash);
  return m_candidateHash;
}

bool State::IsInitialSnapshotDownload() {
  if (m_isdLatch.load(std::memory_order_relaxed)) {
    return false;
  }

  uint256 snapshotHash;
  if (GetLatestFinalizedSnapshotHash(snapshotHash)) {
    m_isdLatch.store(true, std::memory_order_relaxed);
    return false;
  }

  if (chainActive.Height() > 0) {
    // at least one full block is processed, leave ISD
    m_isdLatch.store(true, std::memory_order_relaxed);
    return false;
  }

  return true;
}

void State::HeadersDownloaded() { m_headersDownloaded.store(true); }

bool State::IsHeadersDownloaded() { return m_headersDownloaded.load(); }

State state;

void StoreCandidateBlockHash(const uint256 &hash) {
  state.StoreCandidateBlockHash(hash);
}
uint256 LoadCandidateBlockHash() { return state.LoadCandidateBlockHash(); }
bool IsInitialSnapshotDownload() { return state.IsInitialSnapshotDownload(); }
void EnableISDMode() { state.EnableISDMode(); }
void DisableISDMode() { state.DisableISDMode(); }
bool IsISDEnabled() { return state.IsISDEnabled(); }
void HeadersDownloaded() { state.HeadersDownloaded(); }
bool IsHeadersDownloaded() { return state.IsHeadersDownloaded(); }

}  // namespace snapshot
