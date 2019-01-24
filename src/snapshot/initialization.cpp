// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/initialization.h>

#include <stdint.h>
#include <memory>

#include <snapshot/creator.h>
#include <snapshot/indexer.h>
#include <snapshot/messages.h>
#include <snapshot/p2p_processing.h>
#include <snapshot/snapshot_index.h>
#include <snapshot/state.h>
#include <util.h>
#include <validation.h>

#include <atomic>

namespace snapshot {

namespace {
std::atomic_flag initialized = ATOMIC_FLAG_INIT;
};

bool Initialize(const Params &params) {
  if (initialized.test_and_set()) {
    return error("Already initialized");
  }
  if (!InitSecp256k1Context()) {
    return error("Can't initialize secp256k1_context for the snapshot hash.");
  }

  LoadSnapshotIndex();

  if (fPruneMode) {
    if (gArgs.GetBoolArg("-isd", false)) {
      EnableISDMode();
      LogPrint(BCLog::SNAPSHOT, "Initial Snapshot Download mode is enabled.\n");
    }

    uint256 snapshot_hash;
    if (GetLatestFinalizedSnapshotHash(snapshot_hash)) {
      LogPrintf("Snapshot was successfully applied.\n");
    } else {
      for (const Checkpoint &p : GetSnapshotCheckpoints()) {
        LOCK(cs_snapshot);
        std::unique_ptr<Indexer> idx = Indexer::Open(p.snapshot_hash);
        if (idx) {
          StoreCandidateBlockHash(idx->GetSnapshotHeader().block_hash);
          LogPrint(BCLog::SNAPSHOT, "Candidate snapshot for the block %s has found.\n",
                   idx->GetSnapshotHeader().block_hash.GetHex());
        }
      }
    }
  } else {
    if (gArgs.GetBoolArg("-isd", false)) {
      return error("-isd flag can't be set if pruning is disabled.\n");
    }
  }

  Creator::Init(params);
  InitP2P(params);

  return true;
}

void Deinitialize() {
  LogPrint(BCLog::SNAPSHOT, "%s invoked\n", __func__);
  if (!initialized.test_and_set()) {
    LogPrint(BCLog::SNAPSHOT, "%s: nothing to do, not initialized.\n", __func__);
    return;
  }
  DestroySecp256k1Context();
  Creator::Deinit();
  SaveSnapshotIndex();
  DeinitP2P();
}

}  // namespace snapshot
