// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_INITIALIZATION_H
#define UNITE_SNAPSHOT_INITIALIZATION_H

#include <txdb.h>
#include <scheduler.h>

namespace snapshot {

// Initialize snapshot module
bool Initialize(CCoinsViewDB* view, CScheduler& scheduler);

}

#endif //UNITE_SNAPSHOT_INITIALIZATION_H
