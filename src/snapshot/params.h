// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_SNAPSHOT_PARAMS_H
#define UNIT_E_SNAPSHOT_PARAMS_H

#include <stdint.h>

namespace snapshot {

struct Params {
  uint16_t createSnapshotPerEpoch = 150;
};

}  // namespace snapshot

#endif  //UNIT_E_SNAPSHOT_PARAMS_H
