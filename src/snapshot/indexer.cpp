// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/indexer.h>

#include <crypto/sha256.h>
#include <util.h>

namespace snapshot {

std::unique_ptr<Indexer> Indexer::Open(const uint256 &snapshot_hash) {
  fs::path dirPath(GetDataDir() / SNAPSHOT_FOLDER / snapshot_hash.GetHex());

  Meta meta;
  {
    CAutoFile file(fsbridge::fopen(dirPath / "meta.dat", "rb"), SER_DISK,
                   CLIENT_VERSION);
    if (file.IsNull()) {
      return nullptr;
    }

    file >> meta;
  }

  std::map<uint32_t, IdxMap> dirIdx;
  {
    CAutoFile file(fsbridge::fopen(dirPath / "index.dat", "rb"), SER_DISK,
                   CLIENT_VERSION);
    if (file.IsNull()) {
      return nullptr;
    }

    file >> dirIdx;
  }

  return std::unique_ptr<Indexer>(new Indexer(meta, std::move(dirIdx)));
}

bool Indexer::Delete(const uint256 &snapshot_hash) {
  fs::path dirPath(GetDataDir() / SNAPSHOT_FOLDER / snapshot_hash.GetHex());
  try {
    fs::remove_all(dirPath);
    return true;
  } catch (const fs::filesystem_error &e) {
    LogPrintf("%s: can't delete snapshot %s. error: %s\n", __func__,
              dirPath.string(), e.what());
    return false;
  }
}

Indexer::Indexer(const uint256 &snapshot_hash, const uint256 &block_hash,
                 const uint256 &stake_modifier, const uint256 &chain_work,
                 uint32_t step, uint32_t steps_per_file)
    : m_meta(snapshot_hash, block_hash, stake_modifier, chain_work),
      m_stream(SER_DISK, PROTOCOL_VERSION),
      m_fileId(0),
      m_fileMsgs(0),
      m_fileBytes(0),
      m_dirPath(GetDataDir() / SNAPSHOT_FOLDER / snapshot_hash.GetHex()) {
  assert(step > 0);
  assert(steps_per_file > 0);
  m_meta.step = step;
  m_meta.steps_per_file = steps_per_file;

  TryCreateDirectories(m_dirPath);
}

Indexer::Indexer(const Meta &meta, std::map<uint32_t, IdxMap> &&dirIdx)
    : m_meta(meta),
      m_stream(SER_DISK, PROTOCOL_VERSION),
      m_dirIdx(std::move(dirIdx)),
      m_fileId(0),
      m_fileMsgs(0),
      m_fileBytes(0),
      m_dirPath(GetDataDir() / SNAPSHOT_FOLDER / m_meta.snapshot_hash.GetHex()) {
  assert(m_meta.step > 0);
  assert(m_meta.steps_per_file > 0);

  if (!m_dirIdx.empty()) {
    m_fileId = m_dirIdx.rbegin()->first;

    uint64_t s = (m_dirIdx.size() - 1) * m_meta.step * m_meta.steps_per_file;
    m_fileMsgs = static_cast<uint32_t>(m_meta.total_utxo_subsets - s);
    m_fileIdx = m_dirIdx.rbegin()->second;

    if (!m_fileIdx.empty()) {
      // pre-cache to avoid calculation on every write
      m_fileBytes = m_fileIdx.rbegin()->second;
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

bool Indexer::WriteUTXOSubset(const UTXOSubset &utxoSubset) {
  auto fileId = static_cast<uint32_t>(m_meta.total_utxo_subsets /
                                      (m_meta.step * m_meta.steps_per_file));
  if (fileId > m_fileId) {
    if (!FlushFile()) {
      return false;
    }

    // switch to the new file ID
    m_dirIdx[m_fileId] = std::move(m_fileIdx);
    m_stream.clear();
    m_fileIdx.clear();
    m_fileMsgs = 0;
    m_fileBytes = 0;
    m_fileId = fileId;
  }
  m_stream << utxoSubset;
  uint32_t idx = m_fileMsgs / m_meta.step;
  m_fileIdx[idx] = static_cast<uint32_t>(m_stream.size()) + m_fileBytes;

  ++m_meta.total_utxo_subsets;
  ++m_fileMsgs;

  return true;
}

FILE *Indexer::GetClosestIdx(uint64_t subsetIndex, uint32_t &subsetLeftOut,
                             uint64_t &subsetReadOut) {
  auto fileId = static_cast<uint32_t>(subsetIndex /
                                      (m_meta.step * m_meta.steps_per_file));
  if (m_dirIdx.find(fileId) == m_dirIdx.end()) {
    return nullptr;
  }

  IdxMap idxMap = m_dirIdx.at(fileId);
  uint32_t prevCount = fileId * m_meta.step * m_meta.steps_per_file;
  auto index = static_cast<uint32_t>(subsetIndex - prevCount) / m_meta.step;

  if (idxMap.find(index) == idxMap.end()) {
    return nullptr;
  }

  subsetReadOut =
      fileId * m_meta.step * m_meta.steps_per_file + index * m_meta.step;

  if (m_dirIdx.find(fileId + 1) == m_dirIdx.end()) {
    // last file can have less messages than m_step * stepPerFile
    auto msgInFile =
        static_cast<uint32_t>(m_meta.total_utxo_subsets - prevCount);
    subsetLeftOut = msgInFile - index * m_meta.step;
  } else {
    subsetLeftOut =
        m_meta.step * m_meta.steps_per_file - index * m_meta.step;
  }

  fs::path filePath = m_dirPath / FileName(fileId);
  FILE *file = fsbridge::fopen(filePath, "rb");
  if (!file) {
    return nullptr;
  }

  if (index > 0) {
    if (std::fseek(file, idxMap[index - 1], SEEK_SET) != 0) {
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

std::string Indexer::FileName(uint32_t fileId) {
  return "utxo" + std::to_string(fileId) + ".dat";
}

bool Indexer::FlushFile() {
  CAutoFile file(fsbridge::fopen(m_dirPath / FileName(m_fileId), "ab"),
                 SER_DISK, CLIENT_VERSION);
  if (file.IsNull()) {
    return false;
  }

  m_fileBytes += m_stream.size();
  file << m_stream;
  m_stream.clear();

  return true;
}

bool Indexer::FlushIndex() {
  // m_fileIdx is added to the m_dirIdx
  // only when it's time to switch to the new file
  if (!m_fileIdx.empty()) {
    m_dirIdx[m_fileId] = m_fileIdx;
  }

  CAutoFile file(fsbridge::fopen(m_dirPath / "index.dat", "wb"), SER_DISK,
                 CLIENT_VERSION);
  if (file.IsNull()) {
    return false;
  }

  file << m_dirIdx;
  return true;
}

bool Indexer::FlushMeta() {
  CAutoFile file(fsbridge::fopen(m_dirPath / "meta.dat", "wb"), SER_DISK,
                 CLIENT_VERSION);
  if (file.IsNull()) {
    return false;
  }

  file << m_meta;

  return true;
}

}  // namespace snapshot
