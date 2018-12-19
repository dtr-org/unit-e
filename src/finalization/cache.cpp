// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/cache.h>

#include <chainparams.h>
#include <esperanza/finalizationstate.h>
#include <finalization/p2p.h>
#include <snapshot/creator.h>
#include <validation.h>

namespace finalization {
namespace cache {

// It's gonna be moved to finalization namespace. When it happen, remove this line.
using FinalizationState = esperanza::FinalizationState;

namespace {

//! \brief Storage of finalization states
//!
//! This cache keeps track of finalization states corresponding to block indexes.
class Storage {
 public:
  //! \brief Return finalization state for index, if any
  FinalizationState *Find(const CBlockIndex *index);

  //! \brief Try to find, then try to create new state for index.
  //!
  //! `required_parent_status` reflects the minimal status of the parent's state
  //! in case of `OrCreate`,
  FinalizationState *FindOrCreate(const CBlockIndex *index,
                                  FinalizationState::Status required_parent_status);

  //! \brief Return state for genesis block
  FinalizationState *GetGenesisState() const;

  //! \brief Destroy states for indexes with heights less than `height`
  void ClearUntilHeight(blockchain::Height height);

  //! \brief Reset the storage
  void Reset(const esperanza::FinalizationParams &params,
             const esperanza::AdminParams &admin_params);

  //! \brief Reset the cache and initialize empty and confirmed state for the tip.
  //!
  //! It's a workaround for prune mode. We will get rid of it by restoring finalization
  //! state from disk.
  void ResetToTip(const esperanza::FinalizationParams &params,
                  const esperanza::AdminParams &admin_params,
                  const CBlockIndex *index);

  //! \brief Restoring tells whether node is reconstructing finalization state
  bool Restoring() const {
    return m_restoring;
  }

  //! \brief Put new state to the cache, return pointer to it.
  FinalizationState *Set(const CBlockIndex *index, FinalizationState &&state);

  //! \brief Confirm the state, return pointer to it.
  //!
  //! The `state` must be a state processed from the block. This function fetches previous state
  //! of the same index processed from commits, and replaces it by new state. Compare previous
  //! state with new one and set a result in `out_ok`.
  FinalizationState *Confirm(const CBlockIndex *index, FinalizationState &&state, bool *out_ok);

  struct RestoringRAII {
    Storage &s;
    RestoringRAII(Storage &s) : s(s) { s.m_restoring = true; }
    ~RestoringRAII() { s.m_restoring = false; }
  };

 private:
  FinalizationState *Create(const CBlockIndex *index, FinalizationState::Status required_parent_status);

  mutable CCriticalSection cs;
  std::map<const CBlockIndex *, FinalizationState> m_states;
  std::unique_ptr<FinalizationState> m_genesis_state;
  std::atomic<bool> m_restoring;
} g_storage;

}  // namespace

// Storage implementation section

FinalizationState *Storage::Find(const CBlockIndex *index) {
  LOCK(cs);
  if (index == nullptr) {
    return nullptr;
  }
  if (index->nHeight == 0) {
    return GetGenesisState();
  }
  const auto it = m_states.find(index);
  if (it == m_states.end()) {
    return nullptr;
  } else {
    return &it->second;
  }
}

FinalizationState *Storage::Create(const CBlockIndex *index,
                                   FinalizationState::Status required_parent_status) {
  AssertLockHeld(cs);
  if (index->pprev == nullptr) {
    return nullptr;
  }
  const auto parent_state = Find(index->pprev);
  if ((parent_state == nullptr) ||
      (parent_state != GetGenesisState() && parent_state->GetStatus() < required_parent_status)) {
    return nullptr;
  }
  const auto res = m_states.emplace(index, FinalizationState(*parent_state));
  return &res.first->second;
}

FinalizationState *Storage::FindOrCreate(const CBlockIndex *index,
                                         FinalizationState::Status required_parent_status) {
  LOCK(cs);
  if (const auto state = Find(index)) {
    return state;
  }
  return Create(index, required_parent_status);
}

void Storage::Reset(const esperanza::FinalizationParams &params,
                    const esperanza::AdminParams &admin_params) {
  LOCK(cs);
  m_states.clear();
  m_genesis_state.reset(new FinalizationState(params, admin_params));
}

void Storage::ResetToTip(const esperanza::FinalizationParams &params,
                         const esperanza::AdminParams &admin_params,
                         const CBlockIndex *index) {
  LOCK(cs);
  Reset(params, admin_params);
  m_states.emplace(index, FinalizationState(*GetGenesisState(), FinalizationState::CONFIRMED));
}

void Storage::ClearUntilHeight(blockchain::Height height) {
  LOCK(cs);
  for (auto it = m_states.begin(); it != m_states.end();) {
    const auto index = it->first;
    if (static_cast<blockchain::Height>(index->nHeight) < height) {
      it = m_states.erase(it);
    } else {
      ++it;
    }
  }
}

FinalizationState *Storage::GetGenesisState() const {
  LOCK(cs);
  return m_genesis_state.get();
}

FinalizationState *Storage::Set(const CBlockIndex *block_index, FinalizationState &&state) {
  assert(block_index != nullptr);
  LOCK(cs);
  const auto res = m_states.emplace(block_index, std::move(state));
  if (res.second) {
    return &res.first->second;
  }
  return nullptr;
}

FinalizationState *Storage::Confirm(const CBlockIndex *block_index, FinalizationState &&state, bool *ok_out) {
  assert(block_index != nullptr);
  LOCK(cs);
  if (ok_out) {
    *ok_out = true;
  }
  const auto it = m_states.find(block_index);
  assert(it != m_states.end());
  const auto &old_state = it->second;
  if (old_state != state) {
    if (ok_out) {
      *ok_out = false;
    }
  }
  m_states.erase(it);
  const auto res = m_states.emplace(block_index, std::move(state));
  assert(res.second);
  return &res.first->second;
}

// Helper functions section

namespace {

bool ProcessNewTipWorker(const CBlockIndex &block_index, const CBlock &block) {
  const auto state = g_storage.FindOrCreate(&block_index, FinalizationState::CONFIRMED);
  if (state == nullptr) {
    LogPrint(BCLog::FINALIZATION, "ERROR: Cannot find or create finalization state for %s\n",
             block_index.GetBlockHash().GetHex());
    return false;
  }

  switch (state->GetStatus()) {
    case FinalizationState::NEW: {
      if (!state->ProcessNewTip(block_index, block)) {
        return false;
      }
      break;
    }

    case FinalizationState::FROM_COMMITS: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s heigh=%d has been processed from commits, confirming...\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      const auto ancestor_state = g_storage.Find(block_index.pprev);
      assert(ancestor_state != nullptr);
      FinalizationState new_state(*ancestor_state);
      new_state.ProcessNewTip(block_index, block);
      bool eq;
      g_storage.Confirm(&block_index, std::move(new_state), &eq);
      if (!eq) {
        // UNIT-E TODO: DoS commits sender.
        LogPrint(BCLog::FINALIZATION, "WARN: After processing the block (%s), its finalization state differs from one given from commits. Overwrite it anyway.\n",
                 block_index.GetBlockHash().GetHex());
      } else {
        LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d confirmed\n",
                 block_index.GetBlockHash().GetHex(), block_index.nHeight);
      }
      break;
    }

    case FinalizationState::CONFIRMED: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      return true;
    }
  }

  if (!g_storage.Restoring()) {
    finalization::p2p::OnBlock(block.GetHash());
  }
  return true;
}

bool FinalizationHappened(const CBlockIndex &block_index, blockchain::Height *out_height) {
  const auto *prev_state = GetState(*block_index.pprev);
  const auto *new_state = GetState(block_index);
  if (prev_state != nullptr && new_state != nullptr &&
      block_index.nHeight % new_state->GetEpochLength() == 0) {
    const auto prev_fin_epoch = prev_state->GetLastFinalizedEpoch();
    const auto new_fin_epoch = new_state->GetLastFinalizedEpoch();
    if ((new_fin_epoch != prev_fin_epoch) ||
        (new_fin_epoch == 0 && new_state->IsFinalizedCheckpoint(new_state->GetEpochLength() - 1))) {
      assert(new_fin_epoch > prev_fin_epoch || new_fin_epoch == 0);
      if (out_height != nullptr) {
        *out_height = (new_fin_epoch + 1) * new_state->GetEpochLength() - 1;
      }
      return true;
    }
  }
  return false;
}

void TrimCache(blockchain::Height height) {
  LogPrint(BCLog::FINALIZATION, "Trimming finalization cache for height < %d\n", height);
  g_storage.ClearUntilHeight(height);
}

void FinalizeSnapshot(blockchain::Height height) {
  snapshot::Creator::FinalizeSnapshots(chainActive[height]);
}

}  // namespace

// Global functions section

FinalizationState *GetState() {
  const auto index = chainActive.Tip();
  if (index == nullptr) {
    return nullptr;
  }
  return GetState(*index);
}

FinalizationState *GetState(const CBlockIndex &block_index) {
  return g_storage.Find(&block_index);
}

bool ProcessNewTip(const CBlockIndex &block_index, const CBlock &block) {
  LogPrint(BCLog::FINALIZATION, "Process tip block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);
  if (!ProcessNewTipWorker(block_index, block)) {
    return false;
  }
  if (block_index.nHeight > 0 && !g_storage.Restoring() && (block_index.nHeight + 2) % esperanza::GetEpochLength() == 0) {
    // Generate the snapshot for the block which is one block behind the last one.
    // The last epoch block will contain the snapshot hash pointing to this snapshot.
    snapshot::Creator::GenerateOrSkip(esperanza::GetCurrentEpoch());
  }
  blockchain::Height finalization_height = 0;
  if (FinalizationHappened(block_index, &finalization_height)) {
    TrimCache(finalization_height);
    FinalizeSnapshot(finalization_height);
  }
  return true;
}

bool ProcessNewTipCandidate(const CBlockIndex &block_index, const CBlock &block) {
  LogPrint(BCLog::FINALIZATION, "Process candidate tip block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);
  return ProcessNewTipWorker(block_index, block);
}

bool ProcessNewCommits(const CBlockIndex &block_index, const std::vector<CTransactionRef> &txes) {
  LogPrint(BCLog::FINALIZATION, "Process commits block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);
  const auto state = g_storage.FindOrCreate(&block_index, FinalizationState::FROM_COMMITS);
  if (state == nullptr) {
    LogPrint(BCLog::FINALIZATION, "ERROR: Cannot find or create finalization state for %s\n",
             block_index.GetBlockHash().GetHex());
    return false;
  }

  switch (state->GetStatus()) {
    case esperanza::FinalizationState::NEW: {
      return state->ProcessNewCommits(block_index, txes);
    };
    case esperanza::FinalizationState::FROM_COMMITS: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed from commits\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      return true;
    };
    case esperanza::FinalizationState::CONFIRMED: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      return true;
    };
  }
  // gcc
  assert(not("unreachable"));
  return false;
}

// In this version we read all the blocks from the disk.
// This function might be significantly optimized by using finalization
// state serialization.
void Restore(const CChainParams &chainparams) {
  Storage::RestoringRAII restoring(g_storage);
  if (fPruneMode) {
    if (chainActive.Tip() != nullptr) {
      g_storage.ResetToTip(chainparams.GetFinalization(),
                           chainparams.GetAdminParams(),
                           chainActive.Tip());
    } else {
      g_storage.Reset(chainparams.GetFinalization(), chainparams.GetAdminParams());
    }
    return;
  }

  LogPrint(BCLog::FINALIZATION, "Restore finalization state from disk\n");
  g_storage.Reset(chainparams.GetFinalization(), chainparams.GetAdminParams());
  for (int i = 1; i <= chainActive.Height(); ++i) {
    const CBlockIndex *const index = chainActive[i];
    CBlock block;
    if (!ReadBlockFromDisk(block, index, chainparams.GetConsensus())) {
      assert(not("Failed to read block"));
    }
    ProcessNewTip(*index, block);
  }
}

void Reset(const esperanza::FinalizationParams &params,
           const esperanza::AdminParams &admin_params) {
  g_storage.Reset(params, admin_params);
}

}  // namespace cache
}  // namespace finalization
