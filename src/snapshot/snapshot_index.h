// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_CURRENT_SNAPSHOTS_H
#define UNIT_E_CURRENT_SNAPSHOTS_H

#include <chain.h>
#include <serialize.h>
#include <snapshot/indexer.h>
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
  int height = 0;
  bool finalized = false;
  uint256 snapshot_hash;
  uint256 block_hash;

  Checkpoint() = default;

  Checkpoint(const int _height, const uint256 &_snapshot_hash, const uint256 &_block_hash)
      : height(_height),
        snapshot_hash(_snapshot_hash),
        block_hash(_block_hash) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(height);
    READWRITE(finalized);
    READWRITE(snapshot_hash);
    READWRITE(block_hash);
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
  explicit SnapshotIndex(const uint32_t max_snapshots, const uint32_t min_finalized_snapshots,
                         const bool sanity_check = false)
      : m_max_snapshots(max_snapshots),
        m_min_finalized_snapshots(min_finalized_snapshots),
        m_sanity_check(sanity_check) {
    assert(m_min_finalized_snapshots > 0);
    assert(m_min_finalized_snapshots < m_max_snapshots);
  }

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_index_map);
    READWRITE(m_snapshots_for_removal);
  }

  //! Adds snapshot hash to the index
  //!
  //! \param snapshot_hash that must be added
  //! \param block_index that snapshotHash is referenced to
  //! \return the list of snapshots for removal. After removing each snapshot
  //! it must be confirmed via ConfirmRemoved to prevent returning it again
  std::vector<uint256> AddSnapshotHash(const uint256 &snapshot_hash,
                                       const CBlockIndex *block_index);

  bool GetSnapshotHash(const CBlockIndex *block_index,
                       uint256 &snapshot_hash_out);

  //! returns all available checkpoints at which snapshot was created
  std::vector<Checkpoint> GetSnapshotCheckpoints();

  //! Confirms that the snapshot was removed from disk
  //! and now can be removed from the index
  void ConfirmRemoved(const uint256 &snapshot_hash);

  //! Marks snapshots of the same branch as the block
  //! up to the block height finalized
  //!
  //! \param block_index is the last one of finalized epoch
  //! \return the list of snapshots for removal. After removing each snapshot
  //! it must be confirmed via ConfirmRemoved to prevent returning it again
  std::vector<uint256> FinalizeSnapshots(const CBlockIndex *block_index);

  bool GetLatestFinalizedSnapshotHash(uint256 &snapshot_hash_out);

  //! Returns Indexer if it is registered in g_snapshotIndex
  //!
  //! \param snapshot_hash which should be opened
  //! \return Indexer if snapshot exists and registered in the index
  static std::unique_ptr<Indexer> OpenSnapshot(const uint256 &snapshot_hash);

  //! Deletes snapshot from disk
  static void DeleteSnapshot(const uint256 &snapshot_hash);

  // used in tests only
  static void Clear();

 private:
  //! maximum snapshots to keep
  uint32_t m_max_snapshots;

  //! minimum finalized snapshots to keep
  uint32_t m_min_finalized_snapshots;

  //! sanity check, disabled by default
  bool m_sanity_check;

  //! controls synchronization of functions
  CCriticalSection m_cs;

  //! keeps track of available snapshot hashes
  //! key - block height the snapshot hash points to
  //! value - block and snapshot hash
  std::map<int, Checkpoint> m_index_map;

  //! snapshots that must be confirmed that removed from disk
  std::set<uint256> m_snapshots_for_removal;

  //! returns snapshots which must be removed
  std::vector<uint256> SnapshotsForRemoval();

  //! removes the lowest height preserving m_minFinalizedSnapshots
  void RemoveLowest();

  //! removes the highest snapshot
  void RemoveHighest();

  //! removes snapshotHash from index
  void DeleteSnapshotHash(const uint256 &snapshot_hash);

  void SanityCheck();
};

//! loads index from chainstate DB into memory
void LoadSnapshotIndex();

//! saves index into chainstate DB
void SaveSnapshotIndex();

//! proxy to g_snapshotIndex.AddSnapshotHash()
std::vector<uint256> AddSnapshotHash(const uint256 &snapshot_hash,
                                     const CBlockIndex *block_index);

//! proxy to g_snapshotIndex.GetSnapshotHash()
bool GetSnapshotHash(const CBlockIndex *block_index,
                     uint256 &snapshot_hash_out);

//! proxy to g_snapshotIndex.GetSnapshotCheckpoints()
std::vector<Checkpoint> GetSnapshotCheckpoints();

//! proxy to g_snapshotIndex.ConfirmRemoved()
void ConfirmRemoved(const uint256 &snapshot_hash);

//! proxy to g_snapshotIndex.GetLatestFinalizedSnapshotHash()
bool GetLatestFinalizedSnapshotHash(uint256 &snapshot_hash_out);

//! proxy to g_snapshotIndex.FinalizeSnapshots()
void FinalizeSnapshots(const CBlockIndex *block_index);

}  // namespace snapshot

#endif  //UNIT_E_CURRENT_SNAPSHOTS_H
