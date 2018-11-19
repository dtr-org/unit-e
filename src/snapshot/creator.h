// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_CREATOR_H
#define UNITE_SNAPSHOT_CREATOR_H

#include <stdint.h>
#include <string>

#include <better-enums/enum.h>
#include <scheduler.h>
#include <snapshot/indexer.h>
#include <streams.h>
#include <txdb.h>

namespace snapshot {

// clang-format off
BETTER_ENUM(
  Status,
  uint8_t,
  OK,
  WRITE_ERROR,                // filesystem issue
  RESERVE_SNAPSHOT_ID_ERROR,  // chainparams DB issue
  SET_SNAPSHOT_ID_ERROR,      // chainparams DB issue
  SET_ALL_SNAPSHOTS_ERROR,    // chainparams DB issue
  CALC_SNAPSHOT_HASH_ERROR    // can't calculate the hash
)
// clang-format on

struct CreationInfo {
  Status m_status;
  Meta m_indexerMeta;
  int m_totalOutputs;

  CreationInfo() : m_status(Status::OK), m_totalOutputs(0) {}
};

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
  static void Init(CCoinsViewDB *view, CScheduler &scheduler);

  explicit Creator(CCoinsViewDB *view);
  CreationInfo Create();

 private:
  CCoinsViewDB *m_view;

  //! \brief Generate calls Create() recurrently in the thread
  void Generate();
};

}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_CREATOR_H
