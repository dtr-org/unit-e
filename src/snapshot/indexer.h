// Copyright (c) 2018-2019 The Unit-e developers
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

#include <arith_uint256.h>
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
//! | size | type    | field        | description
//! | 32   | uint256 | tx_id        | TX ID that contains UTXOs
//! | 4    | uint32  | height       | at which bloch height the TX was created
//! | 1    | bool    | is_coin_base |
//! | N    | varInt  | size         | size of the map
//! | 4    | uint32  | key          | CTxOut index
//! | N    | CTxOut  | value        | contains amount and script
//!
//! utxo???.dat file has an incremental suffix starting from 0.
//! File doesn't contain the length of messages/bytes that needs to be read.
//! This info should be taken from the index

constexpr uint32_t DEFAULT_INDEX_STEP = 1000;
constexpr uint32_t DEFAULT_INDEX_STEP_PER_FILE = 100;
const char *const SNAPSHOT_FOLDER = "snapshots";

struct Meta {
  SnapshotHeader snapshot_header;
  uint32_t step = 0;
  uint32_t steps_per_file = 0;

  Meta() = default;

  explicit Meta(const SnapshotHeader &_snapshot_header)
      : snapshot_header(_snapshot_header) {}

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(snapshot_header);
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

  static std::unique_ptr<Indexer> Open(const uint256 &snapshot_hash);
  static bool Delete(const uint256 &snapshot_hash);

  explicit Indexer(const SnapshotHeader &snapshot_header,
                   uint32_t step, uint32_t steps_per_file);

  const SnapshotHeader &GetSnapshotHeader() const { return m_meta.snapshot_header; }
  bool WriteUTXOSubsets(const std::vector<UTXOSubset> &list);
  bool WriteUTXOSubset(const UTXOSubset &utxo_subset);

  //! \brief GetClosestIdx returns the file which contains the expected
  //! index and adjusts the file cursor as close as possible to the UTXOSubset.
  //!
  //! \param subset_index up to which position the file cursor should be moved
  //! \param subset_left_out how many records left in the current file
  //! \param subset_read_out how many records read (including all files)
  //! \return the file that has the message the subset_index points to
  //! and file cursor is moved as close to subset_index as possible
  FILE *GetClosestIdx(uint64_t subset_index, uint32_t &subset_left_out,
                      uint64_t &subset_read_out);

  //! \brief Flush flushes data in the memory to disk.
  //!
  //! Can be invoked after each write. It's automatically called when it's time
  //! to switch the file. Must be manually invoked after the last
  //! WriteUTXOSubset.
  bool Flush();

 private:
  Meta m_meta;
  CDataStream m_stream;  // stores original messages

  std::map<uint32_t, IdxMap> m_dir_idx;  // fileID, file index
  IdxMap m_file_idx;                     // current opened file. key=index, value=byte size
  uint32_t m_file_id = 0;                // current opened file ID
  uint32_t m_file_msgs = 0;              // messages in the current opened file
  uint32_t m_file_bytes = 0;             // written bytes in the current opened file
  fs::path m_dir_path;

  explicit Indexer(const Meta &meta, std::map<uint32_t, IdxMap> &&dir_idx);

  std::string FileName(uint32_t file_id);

  bool FlushFile();
  bool FlushIndex();
  bool FlushMeta();
};
}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_INDEXER_H
