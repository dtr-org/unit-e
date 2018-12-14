// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_CREATOR_H
#define UNITE_SNAPSHOT_CREATOR_H

#include <better-enums/enum.h>
#include <protocol.h>
#include <scheduler.h>
#include <snapshot/chainstate_iterator.h>
#include <snapshot/indexer.h>
#include <snapshot/params.h>
#include <streams.h>
#include <txdb.h>

#include <stdint.h>
#include <string>

namespace snapshot {

// clang-format off
BETTER_ENUM(
  Status,
  uint8_t,
  OK,
  WRITE_ERROR,              // filesystem issue
  CALC_SNAPSHOT_HASH_ERROR  // can't calculate the hash
)
// clang-format on

struct CreationInfo {
  Status status;
  Meta indexer_meta;
  int total_outputs;

  CreationInfo() : status(Status::OK), total_outputs(0) {}
};

//! Creator class accepts the CCoinsViewDB and takes the cursor of it
//! at the point of object construction. Once the Create() function is called,
//! creator object should be thrown way. It's not designed to be re-used.
class Creator {
 public:
  //! aggregate messages per index
  uint32_t m_step = DEFAULT_INDEX_STEP;

  //! aggregations in one file
  uint32_t m_stepsPerFile = DEFAULT_INDEX_STEP_PER_FILE;

  //! how many UTXOSubset include into the snapshot.
  //! 0 - all of them.
  //! non 0 value is used only for testing.
  uint64_t m_maxUTXOSubsets = 0;

  //! \brief Init Initializes the instance of Creator
  //!
  //! Must be invoked before calling any other snapshot::Snapshot* functions
  static void Init(const Params &params, ServiceFlags &service_flags);

  //! Deallocates resources created by Init()
  static void Deinit();

  explicit Creator(CCoinsViewDB *view);

  //! Creates the snapshot of the current chainstate DB
  CreationInfo Create();

  //! Checks if the snapshot must be created for the current epoch
  //! according to the ChainParams.createSnapshotPerEpoch. Snapshot creation is
  //! performed in a separate thread.
  //!
  //! \@param currentEpoch is the current epoch number which starts from 0
  static void GenerateOrSkip(uint32_t currentEpoch);

  //! Marks snapshots of the same branch as blockIndex
  //! and up to its height finalized
  static void FinalizeSnapshots(const CBlockIndex *blockIndex);

 private:
  ChainstateIterator m_iter;
};

}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_CREATOR_H
