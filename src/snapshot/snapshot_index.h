// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_CURRENT_SNAPSHOTS_H
#define UNIT_E_CURRENT_SNAPSHOTS_H

#include <chain.h>
#include <serialize.h>
#include <sync.h>
#include <uint256.h>

#include <stdint.h>
#include <sync.h>
#include <cassert>
#include <map>
#include <set>
#include <vector>

namespace snapshot {

struct Checkpoint {
  int height;
  bool finalized;
  uint256 snapshotHash;
  uint256 blockHash;

  Checkpoint()
      : height(0),
        finalized(false),
        snapshotHash(),
        blockHash() {}

  Checkpoint(int _height, uint256 _snapshotHash, uint256 _blockHash)
      : height(_height),
        finalized(false),
        snapshotHash(_snapshotHash),
        blockHash(_blockHash) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(height);
    READWRITE(finalized);
    READWRITE(snapshotHash);
    READWRITE(blockHash);
  }
};

//! SnapshotIndex keeps track of all available snapshots
//! All its functions are thread-safe
//!
//! It implements the fixed list. Max size is determined by `maxSnapshots`.
//! Position of snapshots is determined by `height`. When the list is full,
//! new snapshot always pushes out one of the existing snapshots.
//! SnapshotIndex keeps the highest `minFinalizedSnapshots` finalized snapshots.
//!
//! Rules on which snapshot to push out:
//! 1. If new snapshot has the position 0..N-1, N is pushed out
//! 2. If new snapshot matches the height with existing one,
//!    the matched snapshot is pushed out
//! 3. If new snapshot has N position, the most left is pushed out
//! The reason to push out the highest snapshot when we add the lowest one is
//! because it means that we switched to another fork and we want to preserve
//! snapshots of the current active fork.
//!
//! After deleting pushed out snapshots, they must be confirmed via `ConfirmRemoved`.
//! that SnapshotIndex won't return them again when `AddSnapshotHash` is called.
class SnapshotIndex {
 public:
  explicit SnapshotIndex(uint32_t maxSnapshots, uint32_t minFinalizedSnapshots,
                         bool m_sanityCheck = false)
      : m_maxSnapshots(maxSnapshots),
        m_minFinalizedSnapshots(minFinalizedSnapshots),
        m_sanityCheck(m_sanityCheck) {
    assert(m_minFinalizedSnapshots > 0);
    assert(m_minFinalizedSnapshots < m_maxSnapshots);
  }

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_indexMap);
    READWRITE(m_snapshotsForRemoval);
  }

  //! Adds snapshot hash to the index
  //!
  //! \param snapshotHash that must be added
  //! \param blockIndex that snapshotHash is referenced to
  //! \return the list of snapshots for removal. After removing each snapshot
  //! it must be confirmed via ConfirmRemoved to prevent returning it again
  std::vector<uint256> AddSnapshotHash(const uint256 &snapshotHash,
                                       const CBlockIndex *blockIndex);

  bool GetSnapshotHash(const CBlockIndex *blockIndex,
                       uint256 &snapshotHashOut);

  //! returns all available checkpoints at which snapshot was created
  std::vector<Checkpoint> GetSnapshotCheckpoints();

  //! Confirms that the snapshot was removed from disk
  //! and now can be removed from the index
  void ConfirmRemoved(const uint256 &snapshotHash);

  //! Marks snapshots of the same branch as the block
  //! up to the block height finalized
  //!
  //! \param blockIndex is the last one of finalized epoch
  //! \return the list of snapshots for removal. After removing each snapshot
  //! it must be confirmed via ConfirmRemoved to prevent returning it again
  std::vector<uint256> FinalizeSnapshots(const CBlockIndex *blockIndex);

  bool GetLatestFinalizedSnapshotHash(uint256 &snapshotHashOut);

  bool GetFinalizedSnapshotHash(const CBlockIndex *blockIndex,
                                uint256 &snapshotHashOut);

  //! Deletes snapshot from disk
  static void DeleteSnapshot(const uint256 &snapshotHash);

  // used in tests only
  static void Clear();

 private:
  //! maximum snapshots to keep
  uint32_t m_maxSnapshots;

  //! minimum finalized snapshots to keep
  uint32_t m_minFinalizedSnapshots;

  //! sanity check, disabled by default
  bool m_sanityCheck;

  //! controls synchronization of functions
  CCriticalSection m_cs;

  //! keeps track of available snapshot hashes
  //! key - block height the snapshot hash points to
  //! value - block and snapshot hash
  std::map<int, Checkpoint> m_indexMap;

  //! snapshots that must be confirmed that removed from disk
  std::set<uint256> m_snapshotsForRemoval;

  //! returns snapshots which must be removed
  std::vector<uint256> SnapshotsForRemoval();

  //! removes the lowest height preserving m_minFinalizedSnapshots
  void RemoveLowest();

  //! removes the highest snapshot
  void RemoveHighest();

  //! removes snapshotHash from index
  void DeleteSnapshotHash(const uint256 &snapshotHash);

  void SanityCheck();
};

//! loads index from chainstate DB into memory
void LoadSnapshotIndex();

//! saves index into chainstate DB
void SaveSnapshotIndex();

//! proxy to g_snapshotIndex.AddSnapshotHash()
std::vector<uint256> AddSnapshotHash(const uint256 &snapshotHash,
                                     const CBlockIndex *block);

//! proxy to g_snapshotIndex.GetSnapshotHash()
bool GetSnapshotHash(const CBlockIndex *blockIndex,
                     uint256 &snapshotHashOut);

//! proxy to g_snapshotIndex.GetSnapshotCheckpoints()
std::vector<Checkpoint> GetSnapshotCheckpoints();

//! proxy to g_snapshotIndex.ConfirmRemoved()
void ConfirmRemoved(const uint256 &snapshotHash);

//! proxy to g_snapshotIndex.GetLatestFinalizedSnapshotHash()
bool GetLatestFinalizedSnapshotHash(uint256 &snapshotHashOut);

//! proxy to g_snapshotIndex.GetFinalizedSnapshotHashByBlockHash()
bool GetFinalizedSnapshotHash(const CBlockIndex *blockIndex,
                              uint256 &snapshotHashOut);

//! proxy to g_snapshotIndex.FinalizeSnapshots()
void FinalizeSnapshots(const CBlockIndex *blockIndex);

}  // namespace snapshot

#endif  //UNIT_E_CURRENT_SNAPSHOTS_H
