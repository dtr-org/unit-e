// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_INDEXER_H
#define UNITE_SNAPSHOT_INDEXER_H

#include <stdint.h>
#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <clientversion.h>
#include <fs.h>
#include <serialize.h>
#include <snapshot/messages.h>
#include <streams.h>
#include <util.h>
#include <version.h>

namespace snapshot {
//! meta.dat
//! | size | type    | field              | description
//! | 32   | uint256 | m_snapshotHash     |
//! | 32   | uint256 | m_blockHash        | at which block hash the snapshot was
//! |      |         |                    | created
//! | 8    | uint64  | m_totalUTXOSubsets | total number of all UTXO
//! |      |         |                    | subsets
//! | 4    | uint32  | m_step             | number of aggregated UTXO
//! |      |         |                    | subsets
//! | 4    | uint32  | m_stepsPerFile     | number of aggregations per
//! file
//!
//! index.dat
//! | size | type    | field | description
//! | N    | varInt  | size  | size of the map
//! | 4    | uint32  | key   | stores fileID starts from 0
//! | N    | IdxMap  | value | stores file index
//!
//! IdxMap
//! | size | type    | field | description
//! | N    | varInt  | size  | size of the map
//! | 4    | uint32  | key   | index (0, 1, 2, ...)
//! | 4    | uint32  | value | bytes to read from the beginning of the file
//! |      |         |       | until the end of the index
//!
//! Example (m_step 10, m_stepsPerFile 3)
//!
//! To understand in which file the needed required message index is:
//! fileId = neededIndex / (m_step * m_stepsPerFile)
//! fileId = 24 / (10 * 3) = 0.8 = utxo0.dat
//! fileId = 57 / (10 * 3) = 1.9 = utxo1.dat
//! fileId = 63 / (10 * 3) = 2.1 = utxo2.dat
//!
//! Once the file is detected, we update the neededIndex according to its
//! fileId position:
//! neededIndex = neededIndex - m_step * m_stepsPerFile * fileId
//! neededIndex = 15 - 10 * 3 * 0 = 15
//! neededIndex = 57 - 10 * 3 * 1 = 27
//! neededIndex = 63 - 10 * 3 * 2 = 3
//!
//! IdxMap for one file might look like this:
//! 0: 100 // 100 bytes store first 10 messages
//! 1: 250 // 250 bytes store first 20 messages
//! 2: 350 // 350 bytes store first 21-30 messages
//! note: last index might have less than 10 messages if this IdxMap is for the
//! last file.
//!
//! To read the N message from the file, we first find the closest index.
//! closestIndex = neededIndex / step
//! closestIndex = 15 / 10 = 1
//! closestIndex = 27 / 10 = 2
//!
//! To calculate how many bytes to skip from the file:
//! IdxMap[min(closestIndex - 1, 0)]
//!
//! Every index in IdxMap aggregates m_step messages but the last index of
//! the last file can have less then m_step messages. To know exactly the number
//! we use the following formula:
//! lastFullIndex = max((the last index in the last file - 1), 0)
//! subsetExceptLastFile = m_step * m_stepsPerFile * (number of files - 1)
//! subsetInLastIndex = m_totalUTXOSubsets-subsetExceptLastFile-lastFullIndex
//!
//! utxo???.dat is the file that stores m_step*m_stepsPerFile UTXO subsets
//! UTXOSubset
//! | size | type    | field      | description
//! | 32   | uint256 | txId       | TX ID that contains UTXOs
//! | 4    | uint32  | height     | at which bloch height the TX was created
//! | 1    | bool    | isCoinBase |
//! | N    | varInt  | size       | size of the map
//! | 4    | uint32  | key        | CTxOut index
//! | N    | CTxOut  | value      | contains amount and script
//!
//! utxo???.dat file has an incremental suffix starting from 0.
//! File doesn't contain the length of messages/bytes that needs to be read.
//! This info should be taken from the index

constexpr uint32_t DEFAULT_INDEX_STEP = 1000;
constexpr uint32_t DEFAULT_INDEX_STEP_PER_FILE = 100;
const char *const SNAPSHOT_FOLDER = "snapshots";

struct Meta {
  uint256 m_snapshotHash;
  uint256 m_blockHash;
  uint256 m_stakeModifier;
  uint64_t m_totalUTXOSubsets;
  uint32_t m_step;
  uint32_t m_stepsPerFile;

  Meta()
      : m_snapshotHash(),
        m_blockHash(),
        m_stakeModifier(),
        m_totalUTXOSubsets(0),
        m_step(0),
        m_stepsPerFile(0) {}

  Meta(const uint256 &snapshotHash, const uint256 &blockHash, const uint256 &stakeModifier)
      : m_snapshotHash(snapshotHash),
        m_blockHash(blockHash),
        m_stakeModifier(stakeModifier),
        m_totalUTXOSubsets(0),
        m_step(0),
        m_stepsPerFile(0) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_snapshotHash);
    READWRITE(m_blockHash);
    READWRITE(m_stakeModifier);
    READWRITE(m_totalUTXOSubsets);
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

  static std::unique_ptr<Indexer> Open(const uint256 &snapshotHash);
  static bool Delete(const uint256 &snapshotHash);

  explicit Indexer(const uint256 &snapshotHash, const uint256 &blockHash,
                   const uint256 &stakeModifier,
                   uint32_t step, uint32_t stepsPerFile);

  const Meta &GetMeta() { return m_meta; }
  bool WriteUTXOSubsets(const std::vector<UTXOSubset> &list);
  bool WriteUTXOSubset(const UTXOSubset &utxoSubset);

  //! \brief GetClosestIdx returns the file which contains the expected
  //! index and adjusts the file cursor as close as possible to the UTXOSubset.
  //!
  //! \param subsetLeftOut how many records left in the current file
  //! \param subsetReadOut how many records read (including all files)
  FILE *GetClosestIdx(uint64_t subsetIndex, uint32_t &subsetLeftOut,
                      uint64_t &subsetReadOut);

  //! \brief Flush flushes data in the memory to disk.
  //!
  //! Can be invoked after each write. It's automatically called when it's time
  //! to switch the file. Must be manually invoked after the last
  //! WriteUTXOSubset.
  bool Flush();

 private:
  Meta m_meta;
  CDataStream m_stream;  // stores original messages

  IdxMap m_fileIdx;                     // index, byte size
  std::map<uint32_t, IdxMap> m_dirIdx;  // fileID, file index
  uint32_t m_fileId;                    // current file ID
  uint32_t m_fileMsgs;                  // messages in the current file
  uint32_t m_fileBytes;                 // written bytes in the current file.
  fs::path m_dirPath;

  explicit Indexer(const Meta &meta, std::map<uint32_t, IdxMap> &&dirIdx);

  std::string FileName(uint32_t fileId);

  bool FlushFile();
  bool FlushIndex();

  // calculates and updates m_snapshotHash
  // if provided one is null
  bool FlushMeta();
};
}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_INDEXER_H
