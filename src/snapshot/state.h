// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_STATE_H
#define UNITE_SNAPSHOT_STATE_H

#include <atomic>

#include <sync.h>
#include <uint256.h>

namespace snapshot {

//! State can be changed in the following order:
//! 1. ISD enabled
//! 2. all headers are downloaded
//! 3. candidate snapshot downloaded
//! 4. snapshot applied (leave ISD)
class State {
 public:
  State() : m_isd_mode(false), m_isd_latch(false), m_headers_downloaded(false) {}

  void StoreCandidateBlockHash(const uint256 &block_hash);
  uint256 LoadCandidateBlockHash();
  void EnableISDMode() { m_isd_mode = true; }
  void DisableISDMode() { m_isd_mode = false; }
  bool IsISDEnabled() { return m_isd_mode; }

  //! \brief IsInitialSnapshotDownload checks if we are in the ISD mode
  //!
  //! Returns true if node needs to download the initial snapshot or
  //! is in the middle of snapshot downloading. Once it returns false,
  //! it stays in this state for entire live of the node.
  bool IsInitialSnapshotDownload();

  void HeadersDownloaded();
  bool IsHeadersDownloaded();

 private:
  // true if we're running in the Initial Snapshot Download mode.
  std::atomic<bool> m_isd_mode;

  // tracks when we leave ISD
  std::atomic<bool> m_isd_latch;

  // keeps track when all headers are downloaded
  std::atomic<bool> m_headers_downloaded;

  // pre-caches block hash of the candidate snapshot
  // to avoid reading the snapshot from disk
  uint256 m_candidate_block_hash;
  CCriticalSection m_cs_candidate_block_hash;
};

void StoreCandidateBlockHash(const uint256 &block_hash);
uint256 LoadCandidateBlockHash();
bool IsInitialSnapshotDownload();
void EnableISDMode();
void DisableISDMode();
bool IsISDEnabled();
void HeadersDownloaded();
bool IsHeadersDownloaded();

}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_STATE_H
