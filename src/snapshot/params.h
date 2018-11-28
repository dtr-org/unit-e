// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_SNAPSHOT_PARAMS_H
#define UNIT_E_SNAPSHOT_PARAMS_H

#include <stdint.h>

namespace snapshot {

struct Params {
  //! the interval in epochs between snapshots
  uint16_t create_snapshot_per_epoch = 150;

  //! if node hasn't received the valid snapshot chunk from its peers
  //! during this timeout, node falls back to IBD
  int64_t fast_sync_timeout_sec = 30;

  //! time during which the node will ask all the peers about the snapshot
  int64_t discovery_timeout_sec = 120;
};

}  // namespace snapshot

#endif  //UNIT_E_SNAPSHOT_PARAMS_H
