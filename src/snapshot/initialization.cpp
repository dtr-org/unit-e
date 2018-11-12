// Copyright (c) 2018 The Unit-e developers
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

namespace snapshot {

bool Initialize(const Params &params) {
  if (!InitSecp256k1Context()) {
    return error("Can't initialize secp256k1_context for the snapshot hash.");
  }

  if (fPruneMode) {
    if (gArgs.GetBoolArg("-isd", false)) {
      EnableISDMode();
      LogPrintf("Initial Snapshot Download mode is enabled.\n");
    }

    uint256 snapshotHash;
    if (GetLatestFinalizedSnapshotHash(snapshotHash)) {
      LogPrintf("Snapshot was successfully applied.\n");
    } else {
      for (const Checkpoint &p : GetSnapshotCheckpoints()) {
        std::unique_ptr<Indexer> idx = Indexer::Open(p.snapshotHash);
        if (idx) {
          StoreCandidateBlockHash(idx->GetMeta().m_blockHash);
          LogPrintf("Candidate snapshot for the block %s has found.\n",
                    idx->GetMeta().m_blockHash.GetHex());
        }
      }
    }
  } else {
    if (gArgs.GetBoolArg("-isd", false)) {
      return error("-isd flag can't be set if pruning is disabled.\n");
    }
  }

  LoadSnapshotIndex();
  Creator::Init(params);

  return true;
}

void Deinitialize() {
  LogPrint(BCLog::SNAPSHOT, "%s invoked\n", __func__);
  DestroySecp256k1Context();
  Creator::Deinit();
  SaveSnapshotIndex();
}

}  // namespace snapshot
