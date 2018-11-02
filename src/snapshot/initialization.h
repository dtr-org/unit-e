// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_INITIALIZATION_H
#define UNITE_SNAPSHOT_INITIALIZATION_H

#include <scheduler.h>
#include <txdb.h>

namespace snapshot {

//! Initialize snapshot module
bool Initialize(CCoinsViewDB *view, CScheduler &scheduler);

//! Deinitialize cleans up initialized objects
void Deinitialize();

}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_INITIALIZATION_H
