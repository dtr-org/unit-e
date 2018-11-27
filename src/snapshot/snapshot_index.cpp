// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/snapshot_index.h>

#include <snapshot/indexer.h>
#include <txdb.h>
#include <util.h>
#include <validation.h>

#include <algorithm>

namespace snapshot {

// keeps track of current available snapshots
SnapshotIndex g_snapshotIndex(5, 3);

std::vector<uint256> SnapshotIndex::AddSnapshotHash(const uint256 &snapshotHash,
                                                    const CBlockIndex *blockIndex) {
  LOCK(m_cs);

  Checkpoint checkpoint(blockIndex->nHeight, snapshotHash,
                        blockIndex->GetBlockHash());

  if (m_indexMap.empty()) {
    m_indexMap[blockIndex->nHeight] = checkpoint;
    return SnapshotsForRemoval();
  }

  auto it = m_indexMap.find(blockIndex->nHeight);
  if (it != m_indexMap.end()) {
    m_snapshotsForRemoval.emplace(it->second.snapshotHash);
    it = m_indexMap.erase(it);
    m_indexMap.emplace_hint(it, blockIndex->nHeight, checkpoint);
    return SnapshotsForRemoval();
  }

  it = m_indexMap.upper_bound(blockIndex->nHeight);
  bool end = it == m_indexMap.end();
  m_indexMap.emplace_hint(it, blockIndex->nHeight, checkpoint);
  if (end) {
    RemoveLowest();
  } else {
    RemoveHighest();
  }

  return SnapshotsForRemoval();
}

void SnapshotIndex::RemoveLowest() {
  if (m_indexMap.size() <= m_maxSnapshots) {
    return;
  }

  uint32_t finalized = 0;
  for (const auto &p : m_indexMap) {
    if (p.second.finalized) {
      ++finalized;
    }
  }

  if (finalized > m_minFinalizedSnapshots) {
    auto it = m_indexMap.begin();
    m_snapshotsForRemoval.emplace(it->second.snapshotHash);
    it = m_indexMap.erase(it);
    return;
  }

  for (auto it = m_indexMap.begin(); it != m_indexMap.end(); ++it) {
    if (it->second.finalized) {
      continue;
    }

    m_snapshotsForRemoval.emplace(it->second.snapshotHash);
    m_indexMap.erase(it);
    return;
  }
}

void SnapshotIndex::RemoveHighest() {
  if (m_indexMap.size() <= m_maxSnapshots) {
    return;
  }
  auto it = std::prev(m_indexMap.end());
  m_snapshotsForRemoval.emplace(it->second.snapshotHash);
  m_indexMap.erase(it);
}

bool SnapshotIndex::GetSnapshotHash(const CBlockIndex *blockIndex,
                                    uint256 &snapshotHashOut) {
  LOCK(m_cs);

  for (const auto &p : m_indexMap) {
    if (p.second.blockHash == blockIndex->GetBlockHash()) {
      snapshotHashOut = p.second.snapshotHash;
      return true;
    }
  }

  return false;
}

std::vector<Checkpoint> SnapshotIndex::GetSnapshotCheckpoints() {
  LOCK(m_cs);

  std::vector<Checkpoint> list;
  for (const auto &p : m_indexMap) {
    list.emplace_back(p.second);
  }

  return list;
}

void SnapshotIndex::DeleteSnapshotHash(const uint256 &snapshotHash) {
  LOCK(m_cs);

  for (auto it = m_indexMap.begin(); it != m_indexMap.end(); ++it) {
    if (it->second.snapshotHash == snapshotHash) {
      m_indexMap.erase(it);
      break;
    }
  }

  m_snapshotsForRemoval.erase(snapshotHash);
}

std::vector<uint256> SnapshotIndex::SnapshotsForRemoval() {
  if (m_sanityCheck) {
    SanityCheck();
  }

  std::vector<uint256> forRemoval(m_snapshotsForRemoval.begin(),
                                  m_snapshotsForRemoval.end());
  return forRemoval;
}

void SnapshotIndex::ConfirmRemoved(const uint256 &snapshotHash) {
  LOCK(m_cs);

  m_snapshotsForRemoval.erase(snapshotHash);
}

void SnapshotIndex::DeleteSnapshot(const uint256 &snapshotHash) {
  Indexer::Delete(snapshotHash);  // remove from disk
  g_snapshotIndex.DeleteSnapshotHash(snapshotHash);
}

void SnapshotIndex::Clear() {
  for (const auto &c : g_snapshotIndex.GetSnapshotCheckpoints()) {
    g_snapshotIndex.DeleteSnapshotHash(c.snapshotHash);
  }
}

std::vector<uint256> SnapshotIndex::FinalizeSnapshots(const CBlockIndex *blockIndex) {
  LOCK(m_cs);

  if (m_indexMap.empty()) {
    return SnapshotsForRemoval();
  }

  // find index equal or lower the lastEpochBlock height
  auto it = m_indexMap.begin();
  while (it != m_indexMap.end()) {
    if (it->second.finalized) {
      ++it;
      continue;
    }

    if (it->second.height > blockIndex->nHeight) {
      break;
    }

    const CBlockIndex *ancestor = blockIndex->GetAncestor(it->second.height);
    if (*ancestor->phashBlock == it->second.blockHash) {  // same branch
      it->second.finalized = true;
    } else {  // different branch, remove it
      m_snapshotsForRemoval.emplace(it->second.snapshotHash);
      it = m_indexMap.erase(it);
    }
  }

  return SnapshotsForRemoval();
}

bool SnapshotIndex::GetLatestFinalizedSnapshotHash(uint256 &snapshotHashOut) {
  LOCK(m_cs);

  for (auto it = m_indexMap.rbegin(); it != m_indexMap.rend(); ++it) {
    if (it->second.finalized) {
      snapshotHashOut = it->second.snapshotHash;
      return true;
    }
  }

  return false;
}

bool SnapshotIndex::GetFinalizedSnapshotHash(const CBlockIndex *blockIndex,
                                             uint256 &snapshotHashOut) {
  LOCK(m_cs);

  for (auto it = m_indexMap.rbegin(); it != m_indexMap.rend(); ++it) {
    if (it->second.finalized &&
        it->second.blockHash == blockIndex->GetBlockHash()) {
      snapshotHashOut = it->second.snapshotHash;
      return true;
    }
  }

  return false;
}

void SnapshotIndex::SanityCheck() {
  if (m_indexMap.empty()) {
    return;
  }

  assert(m_indexMap.size() <= m_maxSnapshots);

  for (auto it = m_indexMap.begin(); it != m_indexMap.end(); ++it) {
    assert(it->first == it->second.height && "height mismatch");
    if (it != m_indexMap.begin()) {
      assert(std::prev(it)->first < it->first && "incorrect height position");
    }
  }
}

void LoadSnapshotIndex() {
  pcoinsdbview->GetSnapshotIndex(g_snapshotIndex);
  LogPrint(BCLog::SNAPSHOT, "Loaded snapshot index\n");
}

void SaveSnapshotIndex() {
  if (pcoinsdbview->SetSnapshotIndex(g_snapshotIndex)) {
    LogPrint(BCLog::SNAPSHOT, "Saved snapshot index\n");
  } else {
    LogPrint(BCLog::SNAPSHOT, "Can't persist snapshot index\n");
  }
}

std::vector<uint256> AddSnapshotHash(const uint256 &snapshotHash,
                                     const CBlockIndex *block) {
  return g_snapshotIndex.AddSnapshotHash(snapshotHash, block);
}

bool GetSnapshotHash(const CBlockIndex *blockIndex,
                     uint256 &snapshotHashOut) {
  return g_snapshotIndex.GetSnapshotHash(blockIndex, snapshotHashOut);
}

std::vector<Checkpoint> GetSnapshotCheckpoints() {
  return g_snapshotIndex.GetSnapshotCheckpoints();
};

void ConfirmRemoved(const uint256 &snapshotHash) {
  g_snapshotIndex.ConfirmRemoved(snapshotHash);
}

bool GetLatestFinalizedSnapshotHash(uint256 &snapshotHashOut) {
  return g_snapshotIndex.GetLatestFinalizedSnapshotHash(snapshotHashOut);
}

bool GetFinalizedSnapshotHash(const CBlockIndex *blockIndex,
                              uint256 &snapshotHashOut) {
  return g_snapshotIndex.GetFinalizedSnapshotHash(blockIndex,
                                                  snapshotHashOut);
}

void FinalizeSnapshots(const CBlockIndex *blockIndex) {
  g_snapshotIndex.FinalizeSnapshots(blockIndex);
}

}  // namespace snapshot
