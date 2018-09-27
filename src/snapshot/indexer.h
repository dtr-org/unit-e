// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_INDEXER_H
#define UNITE_SNAPSHOT_INDEXER_H

#include <cassert>

#include <clientversion.h>
#include <fs.h>
#include <serialize.h>
#include <snapshot/messages.h>
#include <streams.h>
#include <util.h>
#include <version.h>

namespace snapshot {
// clang-format off
//
// meta.dat
// | size | type    | field           | description
// | 32   | uint256 | m_snapshotHash  |
// | 32   | uint256 | m_bestBlockHash | at which hash the snapshot was created
// | 8    | uint64  | m_totalUTXOSets | total number of UTXO sets the snapshot contains
// | 4    | uint32  | m_step          | number of aggregated UTXO sets
// | 4    | uint32  | m_stepsPerFile  | number of aggregations per file
//
// index.dat
// | size | type    | field | description
// | N    | varInt  | size  | size of the map
// | 4    | uint32  | key   | stores fileID starts from 0
// | N    | IdxMap  | value | stores file index
//
// IdxMap
// | size | type    | field | description
// | N    | varInt  | size  | size of the map
// | 4    | uint32  | key   | index (0, 1, 3, ...)
// | 4    | uint32  | value | bytes to read from the file until the end of the index
//
// Example (m_step 10, m_stepsPerFile 3)
// 0: 100 (bytes)
// 1: 250 (bytes)
// 2: 350 (bytes)
//
// To read the N message (0, 10, 20, ...) from the file, we need to skip IdxMap[N/m_step-1] bytes
//
// Last index might contain less than 10 messages if it's the last file.
// To know how many messages are in the last index:
// lastFullIndex = max((the last index in the last file - m_step), 0)
// m_totalUTXOSets - (m_step * m_stepsPerFile * (files - 1)) - lastFullIndex
//
// utxo???.dat is the file that stores m_step*m_stepsPerFile UTXO sets
// UTXO Set
// | size | type    | field      | description
// | 32   | uint256 | txId       | TX ID that contains UTXOs
// | 4    | uint32  | height     | at which bloch height the TX was created
// | 1    | bool    | isCoinBase |
// | N    | varInt  | size       | size of the map
// | 4    | uint32  | key        | CTxOut index
// | N    | CTxOut  | value      | contains amount and script
//
// utxo???.dat file has an incremental suffix starting from 0.
// File doesn't contain the length of messages/bytes that needs to be read.
// This info should be taken from the index
//
// clang-format on

const uint32_t DEFAULT_INDEX_STEP = 1000;
const uint32_t DEFAULT_INDEX_STEP_PER_FILE = 100;
const char* const SNAPSHOT_FOLDER = "snapshots";

struct Meta {
  uint256 m_snapshotHash;
  uint256 m_bestBlockHash;
  uint64_t m_totalUTXOSets;
  uint32_t m_step;
  uint32_t m_stepsPerFile;

  Meta()
      : m_snapshotHash(),
        m_bestBlockHash(),
        m_totalUTXOSets(0),
        m_step(0),
        m_stepsPerFile(0) {}

  Meta(const uint256& snapshotHash, const uint256& bestBlockHash)
      : m_snapshotHash(snapshotHash),
        m_bestBlockHash(bestBlockHash),
        m_totalUTXOSets{0},
        m_step{0},
        m_stepsPerFile{0} {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream& s, Operation ser_action) {
    READWRITE(m_snapshotHash);
    READWRITE(m_bestBlockHash);
    READWRITE(m_totalUTXOSets);
    READWRITE(m_step);
    READWRITE(m_stepsPerFile);
  }
};

class Indexer {
 public:
  //! key: index starting from 0
  //! value: size of bytes from the beginning of the file (utx???.dat)
  //! until the end of this index.
  using IdxMap = std::map<uint32_t, uint32_t>;

  static std::unique_ptr<Indexer> Open(uint32_t snapshotId);
  static bool Delete(uint32_t snapshotId);

  explicit Indexer(uint32_t snapshotId, const uint256& snapshotHash,
                   const uint256& blockHash, uint32_t step,
                   uint32_t stepsPerFile);

  uint32_t GetSnapshotId() { return m_snapshotId; }
  Meta& GetMeta() { return m_meta; }
  bool WriteUTXOSets(const std::vector<UTXOSet>& list);
  bool WriteUTXOSet(const UTXOSet& utxoSet);

  //! \brief GetClosestIdx returns the file which contains the expected
  //! index and adjusts the file cursor as close as possible to the UTXOSet.
  //!
  //! \param utxoSetLeftOut how many records left in the current file
  //! \param utxoSetReadOut how many records read (including all files)
  FILE* GetClosestIdx(uint64_t utxoSetIndex, uint32_t& utxoSetLeftOut,
                      uint64_t& utxoSetReadOut);

  uint256 CalcSnapshotHash();

  //! \brief Flush flushes data in the memory to disk.
  //!
  //! Can be invoked after each write. It's automatically called when it's time
  //! to switch the file. Must be manually invoked after the last WriteUTXOSet.
  bool Flush();

 private:
  uint32_t m_snapshotId;  // folder in the data dir is equal to ID

  Meta m_meta;
  CDataStream m_stream;  // stores original messages

  IdxMap m_fileIdx;                     // index, byte size
  std::map<uint32_t, IdxMap> m_dirIdx;  // fileID, file index
  uint32_t m_fileId;                    // current file ID
  uint32_t m_fileMsgs;                  // messages in the current file
  uint32_t m_fileBytes;                 // written bytes in the current file.
  fs::path m_dirPath;

  explicit Indexer(uint32_t snapshotId, Meta meta,
                   std::map<uint32_t, IdxMap> dirIdx);

  std::string FileName(uint32_t fileId);

  bool FlushFile();
  bool FlushIndex();

  // calculates and updates m_snapshotHash
  // if provided one is null
  bool FlushMeta();
};
}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_INDEXER_H
