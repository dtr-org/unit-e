// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/indexer.h>

#include <crypto/sha256.h>
#include <util/system.h>

namespace snapshot {

CCriticalSection cs_snapshot;

std::unique_ptr<Indexer> Indexer::Open(const uint256 &snapshot_hash) EXCLUSIVE_LOCKS_REQUIRED(cs_snapshot) {
  AssertLockHeld(cs_snapshot);

  fs::path dir_path(GetDataDir() / SNAPSHOT_FOLDER / snapshot_hash.GetHex());

  Meta meta;
  {
    CAutoFile file(fsbridge::fopen(dir_path / "meta.dat", "rb"), SER_DISK,
                   CLIENT_VERSION);
    if (file.IsNull()) {
      return nullptr;
    }

    file >> meta;
  }

  std::map<uint32_t, IdxMap> dir_idx;
  {
    CAutoFile file(fsbridge::fopen(dir_path / "index.dat", "rb"), SER_DISK,
                   CLIENT_VERSION);
    if (file.IsNull()) {
      return nullptr;
    }

    file >> dir_idx;
  }

  return std::unique_ptr<Indexer>(new Indexer(meta, std::move(dir_idx)));
}

bool Indexer::Delete(const uint256 &snapshot_hash) EXCLUSIVE_LOCKS_REQUIRED(cs_snapshot) {
  AssertLockHeld(cs_snapshot);

  fs::path dir_path(GetDataDir() / SNAPSHOT_FOLDER / snapshot_hash.GetHex());
  try {
    fs::remove_all(dir_path);
    return true;
  } catch (const fs::filesystem_error &e) {
    LogPrintf("%s: can't delete snapshot %s. error: %s\n", __func__,
              dir_path.string(), e.what());
    return false;
  }
}

Indexer::Indexer(const SnapshotHeader &snapshot_header,
                 const uint32_t step, const uint32_t steps_per_file)
    : m_meta(snapshot_header),
      m_stream(SER_DISK, PROTOCOL_VERSION),
      m_dir_path(GetDataDir() / SNAPSHOT_FOLDER / m_meta.snapshot_header.snapshot_hash.GetHex()) {
  assert(step > 0);
  assert(steps_per_file > 0);
  m_meta.snapshot_header.total_utxo_subsets = 0;  // it's incremented after each write
  m_meta.step = step;
  m_meta.steps_per_file = steps_per_file;

  TryCreateDirectories(m_dir_path);
}

Indexer::Indexer(const Meta &meta, std::map<uint32_t, IdxMap> &&dir_idx)
    : m_meta(meta),
      m_stream(SER_DISK, PROTOCOL_VERSION),
      m_dir_idx(std::move(dir_idx)),
      m_dir_path(GetDataDir() / SNAPSHOT_FOLDER / m_meta.snapshot_header.snapshot_hash.GetHex()) {
  assert(m_meta.step > 0);
  assert(m_meta.steps_per_file > 0);

  if (!m_dir_idx.empty()) {
    m_file_id = m_dir_idx.rbegin()->first;

    uint64_t s = (m_dir_idx.size() - 1) * m_meta.step * m_meta.steps_per_file;
    m_file_msgs = static_cast<uint32_t>(m_meta.snapshot_header.total_utxo_subsets - s);
    m_file_idx = m_dir_idx.rbegin()->second;

    if (!m_file_idx.empty()) {
      // pre-cache to avoid calculation on every write
      m_file_bytes = m_file_idx.rbegin()->second;
    }
  }
}

bool Indexer::WriteUTXOSubsets(const std::vector<UTXOSubset> &list) {
  for (const auto &msg : list) {
    if (!WriteUTXOSubset(msg)) {
      return false;
    }
  }
  return true;
}

bool Indexer::WriteUTXOSubset(const UTXOSubset &utxo_subset) {
  auto file_id = static_cast<uint32_t>(m_meta.snapshot_header.total_utxo_subsets /
                                       (m_meta.step * m_meta.steps_per_file));
  if (file_id > m_file_id) {
    if (!FlushFile()) {
      return false;
    }

    // switch to the new file ID
    m_dir_idx[m_file_id] = std::move(m_file_idx);
    m_stream.clear();
    m_file_idx.clear();
    m_file_msgs = 0;
    m_file_bytes = 0;
    m_file_id = file_id;
  }
  m_stream << utxo_subset;
  uint32_t idx = m_file_msgs / m_meta.step;
  m_file_idx[idx] = static_cast<uint32_t>(m_stream.size()) + m_file_bytes;

  ++m_meta.snapshot_header.total_utxo_subsets;
  ++m_file_msgs;

  return true;
}

FILE *Indexer::GetClosestIdx(const uint64_t subset_index, uint32_t &subset_left_out,
                             uint64_t &subset_read_out) {
  auto file_id = static_cast<uint32_t>(subset_index /
                                       (m_meta.step * m_meta.steps_per_file));
  if (m_dir_idx.find(file_id) == m_dir_idx.end()) {
    return nullptr;
  }

  IdxMap idx_map = m_dir_idx.at(file_id);
  uint32_t prev_count = file_id * m_meta.step * m_meta.steps_per_file;
  auto index = static_cast<uint32_t>(subset_index - prev_count) / m_meta.step;

  if (idx_map.find(index) == idx_map.end()) {
    return nullptr;
  }

  subset_read_out =
      file_id * m_meta.step * m_meta.steps_per_file + index * m_meta.step;

  if (m_dir_idx.find(file_id + 1) == m_dir_idx.end()) {
    // last file can have less messages than m_step * stepPerFile
    auto msg_in_file =
        static_cast<uint32_t>(m_meta.snapshot_header.total_utxo_subsets - prev_count);
    subset_left_out = msg_in_file - index * m_meta.step;
  } else {
    subset_left_out =
        m_meta.step * m_meta.steps_per_file - index * m_meta.step;
  }

  fs::path filePath = m_dir_path / FileName(file_id);
  FILE *file = fsbridge::fopen(filePath, "rb");
  if (!file) {
    return nullptr;
  }

  if (index > 0) {
    if (std::fseek(file, idx_map[index - 1], SEEK_SET) != 0) {
      fclose(file);
      return nullptr;
    }
  }

  return file;
}

bool Indexer::Flush() {
  if (!m_stream.empty()) {
    if (!FlushFile()) {
      return false;
    }
  }

  if (!FlushIndex()) {
    return false;
  }

  return FlushMeta();
}

std::string Indexer::FileName(const uint32_t file_id) {
  return "utxo" + std::to_string(file_id) + ".dat";
}

bool Indexer::FlushFile() {
  CAutoFile file(fsbridge::fopen(m_dir_path / FileName(m_file_id), "ab"),
                 SER_DISK, CLIENT_VERSION);
  if (file.IsNull()) {
    return false;
  }

  m_file_bytes += m_stream.size();
  file << m_stream;
  m_stream.clear();

  return true;
}

bool Indexer::FlushIndex() {
  // m_file_idx is added to the m_dir_idx
  // only when it's time to switch to the new file
  if (!m_file_idx.empty()) {
    m_dir_idx[m_file_id] = m_file_idx;
  }

  CAutoFile file(fsbridge::fopen(m_dir_path / "index.dat", "wb"), SER_DISK,
                 CLIENT_VERSION);
  if (file.IsNull()) {
    return false;
  }

  file << m_dir_idx;
  return true;
}

bool Indexer::FlushMeta() {
  CAutoFile file(fsbridge::fopen(m_dir_path / "meta.dat", "wb"), SER_DISK,
                 CLIENT_VERSION);
  if (file.IsNull()) {
    return false;
  }

  file << m_meta;

  return true;
}

}  // namespace snapshot
