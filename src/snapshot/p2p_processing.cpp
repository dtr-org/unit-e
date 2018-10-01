// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/p2p_processing.h>

#include <memory>

#include <txdb.h>
#include <util.h>
#include <validation.h>

bool IsInitialSnapshotDownload() {
  static std::atomic<bool> latch(false);
  if (latch.load(std::memory_order_relaxed)) {
    return false;
  }

  LOCK(cs_main);
  if (latch.load(std::memory_order_relaxed)) {
    return false;
  }

  uint32_t snapshotId;
  if (pcoinsdbview->GetSnapshotId(snapshotId)) {
    LogPrint(BCLog::NET, "Leaving IsInitialSnapshotDownload\n");
    latch.store(true, std::memory_order_relaxed);
    return false;
  }

  return true;
}
