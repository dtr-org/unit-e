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
//! | 32   | uint256 | snapshot_hash      |
//! | 32   | uint256 | block_hash         | at which block hash the snapshot was
//! |      |         |                    | created
//! | 8    | uint64  | total_utxo_subsets | total number of all UTXO
//! |      |         |                    | subsets
//! | 4    | uint32  | step               | number of aggregated UTXO
//! |      |         |                    | subsets
//! | 4    | uint32  | steps_per_file     | number of aggregations per
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
//! Example (step=10, steps_per_file=3)
//!
//! To understand in which file the needed required message index is:
//! file_id = needed_index / (step * steps_per_file)
//! file_id = 24 / (10 * 3) = 0.8 = utxo0.dat
//! file_id = 57 / (10 * 3) = 1.9 = utxo1.dat
//! file_id = 63 / (10 * 3) = 2.1 = utxo2.dat
//!
//! Once the file is detected, we update the needed_index according to its
//! file_id position:
//! needed_index = needed_index - step * steps_per_file * fileId
//! needed_index = 15 - 10 * 3 * 0 = 15
//! needed_index = 57 - 10 * 3 * 1 = 27
//! needed_index = 63 - 10 * 3 * 2 = 3
//!
//! IdxMap for one file might look like this:
//! 0: 100 // 100 bytes store first 10 messages
//! 1: 250 // 250 bytes store first 20 messages
//! 2: 350 // 350 bytes store first 21-30 messages
//! note: last index might have less than 10 messages if this IdxMap is for the
//! last file.
//!
//! To read the N message from the file, we first find the closest index.
//! closest_index = needed_index / step
//! closest_index = 15 / 10 = 1
//! closest_index = 27 / 10 = 2
//!
//! To calculate how many bytes to skip from the file:
//! IdxMap[min(closest_index - 1, 0)]
//!
//! Every index in IdxMap aggregates step messages but the last index of
//! the last file can have less then step messages. To know exactly the number
//! we use the following formula:
//! last_full_index = max((the last index in the last file - 1), 0)
//! subset_except_last_file = step * steps_per_file * (number of files - 1)
//! subset_in_last_index = total_utxo_subsets - subset_except_last_file - last_full_index
//!
//! utxo???.dat is the file that stores step * steps_per_file UTXO subsets
//! UTXOSubset
//! | size | type    | field      | description
//! | 32   | uint256 | txId       | TX ID that contains UTXOs
//! | 4    | uint32  | height     | at which bloch height the TX was created
//! | 1    | bool    | isCoinStake |
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
  uint256 snapshot_hash;
  uint256 block_hash;
  uint256 stake_modifier;
  uint64_t total_utxo_subsets;
  uint32_t step;
  uint32_t steps_per_file;

  Meta()
      : snapshot_hash(),
        block_hash(),
        stake_modifier(),
        total_utxo_subsets(0),
        step(0),
        steps_per_file(0) {}

  Meta(const uint256 &_snapshot_hash, const uint256 &_block_hash, const uint256 &_stake_modifier)
      : snapshot_hash(_snapshot_hash),
        block_hash(_block_hash),
        stake_modifier(_stake_modifier),
        total_utxo_subsets(0),
        step(0),
        steps_per_file(0) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(snapshot_hash);
    READWRITE(block_hash);
    READWRITE(stake_modifier);
    READWRITE(total_utxo_subsets);
    READWRITE(step);
    READWRITE(steps_per_file);
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
