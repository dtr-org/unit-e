// Copyright (c) 2018 The unit-e core developers
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
  State() : m_isdMode(false), m_isdLatch(false), m_headersDownloaded(false){};

  void StoreCandidateBlockHash(uint256 hash);
  uint256 LoadCandidateBlockHash();
  void EnableISDMode() { m_isdMode = true; };
  bool IsISDEnabled() { return m_isdMode; };

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
  bool m_isdMode;

  // tracks when we leave ISD
  std::atomic<bool> m_isdLatch;

  // keeps track when all headers are downloaded
  std::atomic<bool> m_headersDownloaded;

  // pre-caches candidate snapshot hash to avoid lookup to the disk
  uint256 m_candidateHash;
  CCriticalSection cs_candidateBlockHash;
};

void StoreCandidateBlockHash(uint256 hash);
uint256 LoadCandidateBlockHash();
bool IsInitialSnapshotDownload();
void EnableISDMode();
bool IsISDEnabled();
void HeadersDownloaded();
bool IsHeadersDownloaded();

}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_STATE_H
