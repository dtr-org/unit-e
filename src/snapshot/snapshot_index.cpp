// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/snapshot_index.h>

#include <txdb.h>
#include <util/system.h>
#include <validation.h>

#include <algorithm>

namespace snapshot {

// keeps track of current available snapshots
SnapshotIndex g_snapshot_index(5, 2);

std::vector<uint256> SnapshotIndex::AddSnapshotHash(const uint256 &snapshot_hash,
                                                    const CBlockIndex *block_index) {
  LOCK(m_cs);

  Checkpoint checkpoint(block_index->nHeight, snapshot_hash,
                        block_index->GetBlockHash());

  if (m_index_map.empty()) {
    m_index_map[block_index->nHeight] = checkpoint;
    return SnapshotsForRemoval();
  }

  auto it = m_index_map.find(block_index->nHeight);
  if (it != m_index_map.end()) {
    m_snapshots_for_removal.emplace(it->second.snapshot_hash);
    it = m_index_map.erase(it);
    m_index_map.emplace_hint(it, block_index->nHeight, checkpoint);
    return SnapshotsForRemoval();
  }

  it = m_index_map.upper_bound(block_index->nHeight);
  bool end = it == m_index_map.end();
  m_index_map.emplace_hint(it, block_index->nHeight, checkpoint);
  if (end) {
    RemoveLowest();
  } else {
    RemoveHighest();
  }

  return SnapshotsForRemoval();
}

void SnapshotIndex::RemoveLowest() {
  if (m_index_map.size() <= m_max_snapshots) {
    return;
  }

  uint32_t finalized = 0;
  for (const auto &p : m_index_map) {
    if (p.second.finalized) {
      ++finalized;
    }
  }

  if (finalized > m_min_finalized_snapshots) {
    auto it = m_index_map.begin();
    m_snapshots_for_removal.emplace(it->second.snapshot_hash);
    it = m_index_map.erase(it);
    return;
  }

  for (auto it = m_index_map.begin(); it != m_index_map.end(); ++it) {
    if (it->second.finalized) {
      continue;
    }

    m_snapshots_for_removal.emplace(it->second.snapshot_hash);
    m_index_map.erase(it);
    return;
  }
}

void SnapshotIndex::RemoveHighest() {
  if (m_index_map.size() <= m_max_snapshots) {
    return;
  }
  auto it = std::prev(m_index_map.end());
  m_snapshots_for_removal.emplace(it->second.snapshot_hash);
  m_index_map.erase(it);
}

bool SnapshotIndex::GetSnapshotHash(const CBlockIndex *block_index,
                                    uint256 &snapshot_hash_out) {
  LOCK(m_cs);

  for (const auto &p : m_index_map) {
    if (p.second.block_hash == block_index->GetBlockHash()) {
      snapshot_hash_out = p.second.snapshot_hash;
      return true;
    }
  }

  return false;
}

std::vector<Checkpoint> SnapshotIndex::GetSnapshotCheckpoints() {
  LOCK(m_cs);

  std::vector<Checkpoint> list;
  for (const auto &p : m_index_map) {
    list.emplace_back(p.second);
  }

  return list;
}

void SnapshotIndex::DeleteSnapshotHash(const uint256 &snapshot_hash) {
  LOCK(m_cs);

  for (auto it = m_index_map.begin(); it != m_index_map.end(); ++it) {
    if (it->second.snapshot_hash == snapshot_hash) {
      m_index_map.erase(it);
      break;
    }
  }

  m_snapshots_for_removal.erase(snapshot_hash);
}

std::vector<uint256> SnapshotIndex::SnapshotsForRemoval() {
  if (m_sanity_check) {
    SanityCheck();
  }

  std::vector<uint256> for_removal(m_snapshots_for_removal.begin(),
                                   m_snapshots_for_removal.end());
  return for_removal;
}

void SnapshotIndex::ConfirmRemoved(const uint256 &snapshot_hash) {
  LOCK(m_cs);

  m_snapshots_for_removal.erase(snapshot_hash);
}

std::unique_ptr<Indexer> SnapshotIndex::OpenSnapshot(const uint256 &snapshot_hash)
    EXCLUSIVE_LOCKS_REQUIRED(cs_snapshot) {
  AssertLockHeld(cs_snapshot);

  for (Checkpoint &p : snapshot::GetSnapshotCheckpoints()) {
    if (p.snapshot_hash == snapshot_hash) {
      return Indexer::Open(snapshot_hash);
    }
  }

  return nullptr;
}

void SnapshotIndex::DeleteSnapshot(const uint256 &snapshot_hash)
    EXCLUSIVE_LOCKS_REQUIRED(cs_snapshot) {
  AssertLockHeld(cs_snapshot);

  Indexer::Delete(snapshot_hash);  // remove from disk
  g_snapshot_index.DeleteSnapshotHash(snapshot_hash);
}

void SnapshotIndex::Clear() {
  for (const auto &c : g_snapshot_index.GetSnapshotCheckpoints()) {
    g_snapshot_index.DeleteSnapshotHash(c.snapshot_hash);
  }
}

std::vector<uint256> SnapshotIndex::FinalizeSnapshots(const CBlockIndex *block_index) {
  LOCK(m_cs);

  if (m_index_map.empty()) {
    return SnapshotsForRemoval();
  }

  // find index equal or lower the lastEpochBlock height
  auto it = m_index_map.begin();
  while (it != m_index_map.end()) {
    if (it->second.finalized) {
      ++it;
      continue;
    }

    if (it->second.height > block_index->nHeight) {
      break;
    }

    const CBlockIndex *ancestor = block_index->GetAncestor(it->second.height);
    if (*ancestor->phashBlock == it->second.block_hash) {  // same branch
      it->second.finalized = true;
    } else {  // different branch, remove it
      m_snapshots_for_removal.emplace(it->second.snapshot_hash);
      it = m_index_map.erase(it);
    }
  }

  return SnapshotsForRemoval();
}

bool SnapshotIndex::GetLatestFinalizedSnapshotHash(uint256 &snapshot_hash_out) {
  LOCK(m_cs);

  for (auto it = m_index_map.rbegin(); it != m_index_map.rend(); ++it) {
    if (it->second.finalized) {
      snapshot_hash_out = it->second.snapshot_hash;
      return true;
    }
  }

  return false;
}

void SnapshotIndex::SanityCheck() {
  if (m_index_map.empty()) {
    return;
  }

  assert(m_index_map.size() <= m_max_snapshots);

  for (auto it = m_index_map.begin(); it != m_index_map.end(); ++it) {
    assert(it->first == it->second.height && "height mismatch");
    if (it != m_index_map.begin()) {
      assert(std::prev(it)->first < it->first && "incorrect height position");
    }
  }
}

void LoadSnapshotIndex() {
  pcoinsdbview->GetSnapshotIndex(g_snapshot_index);
  LogPrint(BCLog::SNAPSHOT, "Loaded snapshot index\n");
}

void SaveSnapshotIndex() {
  if (pcoinsdbview->SetSnapshotIndex(g_snapshot_index)) {
    LogPrint(BCLog::SNAPSHOT, "Saved snapshot index\n");
  } else {
    LogPrint(BCLog::SNAPSHOT, "Can't persist snapshot index\n");
  }
}

std::vector<uint256> AddSnapshotHash(const uint256 &snapshot_hash,
                                     const CBlockIndex *block_index) {
  return g_snapshot_index.AddSnapshotHash(snapshot_hash, block_index);
}

bool GetSnapshotHash(const CBlockIndex *block_index,
                     uint256 &snapshot_hash_out) {
  return g_snapshot_index.GetSnapshotHash(block_index, snapshot_hash_out);
}

std::vector<Checkpoint> GetSnapshotCheckpoints() {
  return g_snapshot_index.GetSnapshotCheckpoints();
}

void ConfirmRemoved(const uint256 &snapshot_hash) {
  g_snapshot_index.ConfirmRemoved(snapshot_hash);
}

bool GetLatestFinalizedSnapshotHash(uint256 &snapshot_hash_out) {
  return g_snapshot_index.GetLatestFinalizedSnapshotHash(snapshot_hash_out);
}

void FinalizeSnapshots(const CBlockIndex *block_index) {
  g_snapshot_index.FinalizeSnapshots(block_index);
}

}  // namespace snapshot
