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
#include <snapshot/state.h>
#include <util.h>
#include <validation.h>

namespace snapshot {

bool Initialize(CCoinsViewDB *view, const Params &params) {
  if (!InitSecp256k1Context()) {
    return error("Can't initialize secp256k1_context for the snapshot hash.");
  }

  if (fPruneMode) {
    if (gArgs.GetBoolArg("-isd", false)) {
      EnableISDMode();
      LogPrintf("Initial Snapshot Download mode is enabled.\n");
    }

    uint32_t id = 0;
    if (view->GetSnapshotId(id)) {
      LogPrintf("Snapshot was successfully applied.\n");
    } else {
      if (view->GetCandidateSnapshotId(id)) {
        std::unique_ptr<Indexer> idx = Indexer::Open(id);
        if (idx) {
          StoreCandidateBlockHash(idx->GetMeta().m_bestBlockHash);
          LogPrintf("Candidate snapshot for the block %s has found.\n",
                    idx->GetMeta().m_bestBlockHash.GetHex());
        }
      }
    }
  } else {
    if (gArgs.GetBoolArg("-isd", false)) {
      return error("-isd flag can't be set if pruning is disabled.\n");
    }
  }

  Creator::Init(params);

  return true;
}

void Deinitialize() {
  LogPrint(BCLog::SNAPSHOT, "%s invoked\n", __func__);
  DestroySecp256k1Context();
  Creator::Deinit();
}

}  // namespace snapshot
