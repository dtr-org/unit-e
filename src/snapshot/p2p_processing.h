// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_P2P_PROCESSING_H
#define UNITE_SNAPSHOT_P2P_PROCESSING_H

//! \brief IsInitialSnapshotDownload checks if we are in the ISD mode
//!
//! Returns true if node needs to download the initial snapshot or
//! is in the middle of snapshot downloading. Once it returns false,
//! it stays in this state for entire live of the node.
bool IsInitialSnapshotDownload();

#endif //UNITE_SNAPSHOT_P2P_PROCESSING_H
