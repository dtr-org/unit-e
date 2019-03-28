// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_SNAPSHOT_PARAMS_H
#define UNIT_E_SNAPSHOT_PARAMS_H

#include <cstdint>

namespace snapshot {

struct Params {
  //! the interval in epochs between snapshots
  uint16_t create_snapshot_per_epoch = 150;

  //! if the peer is not able to provide a valid chunk withing this timeout
  //! this peer is marked as it doesn't have a snapshot
  int64_t snapshot_chunk_timeout_sec = 30;

  //! time during which the node will discover available snapshots from peers.
  //! peers joined after this timeout won't be asked for the snapshot
  int64_t discovery_timeout_sec = 120;
};

}  // namespace snapshot

#endif  //UNIT_E_SNAPSHOT_PARAMS_H
