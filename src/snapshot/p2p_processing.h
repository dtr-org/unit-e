// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_P2P_SNAPSHOT_PROCESSING_H
#define UNIT_E_P2P_SNAPSHOT_PROCESSING_H

// Returns true if node needs to download the initial snapshot or
// is in the middle of snapshot downloading. Once it returns false,
// it stays in this state for entire live of the node.
bool IsInitialSnapshotDownload();

#endif //UNIT_E_P2P_SNAPSHOT_PROCESSING_H
