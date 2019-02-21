// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/state_storage.h>

#include <chainparams.h>
#include <esperanza/finalizationstate.h>
#include <finalization/p2p.h>
#include <snapshot/creator.h>
#include <validation.h>

namespace finalization {

// UNIT-E TODO: FinalizationState is gonna be moved to finalization namespace.
// When it happen, remove this line.
using FinalizationState = esperanza::FinalizationState;

namespace {

//! \brief Underlying storage of finalization states
//!
//! This storage keeps track of finalization states corresponding to block indexes.
class Storage {
 public:
  //! \brief Return finalization state for index, if any
  FinalizationState *Find(const CBlockIndex &index);

  //! \brief Try to find, then try to create new state for index.
  //!
  //! `required_parent_status` reflects the minimal status of the parent's state
  //! in case of `OrCreate`,
  FinalizationState *FindOrCreate(const CBlockIndex &index,
                                  FinalizationState::InitStatus required_parent_status);

  //! \brief Return state for genesis block
  FinalizationState *GetGenesisState() const;

  //! \brief Destroy states for indexes with heights less than `height`
  void ClearUntilHeight(blockchain::Height height);

  //! \brief Reset the storage
  void Reset(const esperanza::FinalizationParams &params,
             const esperanza::AdminParams &admin_params);

  //! \brief Reset the storage and initialize empty and confirmed state for the tip.
  //!
  //! It's a workaround for prune mode. We will get rid of it by restoring finalization
  //! state from disk.
  void ResetToTip(const esperanza::FinalizationParams &params,
                  const esperanza::AdminParams &admin_params,
                  const CBlockIndex &index);

  //! \brief Restoring tells whether node is reconstructing finalization state
  bool Restoring() const {
    return m_restoring;
  }

  //! \brief Put new state to the storage, return pointer to it.
  FinalizationState *Set(const CBlockIndex &index, FinalizationState &&state);

  //! \brief Confirm the state.
  //!
  //! The `state` must be a state processed from the block. This function fetches previous state
  //! of the same index processed from commits, and replaces it by new state. Return the result
  //! of comparison between new and previous state.
  bool Confirm(const CBlockIndex &index, FinalizationState &&new_state, FinalizationState **state_out);

  struct RestoringRAII {
    Storage &s;
    explicit RestoringRAII(Storage &s) : s(s) { s.m_restoring = true; }
    ~RestoringRAII() { s.m_restoring = false; }
  };

 private:
  FinalizationState *Create(const CBlockIndex &index, FinalizationState::InitStatus required_parent_status);

  mutable CCriticalSection cs;
  std::map<const CBlockIndex *, FinalizationState> m_states;
  std::unique_ptr<FinalizationState> m_genesis_state;
  std::atomic<bool> m_restoring{false};
};

// Storage implementation section

FinalizationState *Storage::Find(const CBlockIndex &index) {
  LOCK(cs);
  if (index.nHeight == 0) {
    return GetGenesisState();
  }
  const auto it = m_states.find(&index);
  if (it == m_states.end()) {
    return nullptr;
  }
  return &it->second;
}

FinalizationState *Storage::Create(const CBlockIndex &index,
                                   FinalizationState::InitStatus required_parent_status) {
  AssertLockHeld(cs);
  if (index.pprev == nullptr) {
    return nullptr;
  }
  const auto parent_state = Find(*index.pprev);
  if ((parent_state == nullptr) ||
      (parent_state != GetGenesisState() && parent_state->GetInitStatus() < required_parent_status)) {
    return nullptr;
  }
  const auto res = m_states.emplace(&index, FinalizationState(*parent_state));
  return &res.first->second;
}

FinalizationState *Storage::FindOrCreate(const CBlockIndex &index,
                                         FinalizationState::InitStatus required_parent_status) {
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
                         const CBlockIndex &index) {
  LOCK(cs);
  Reset(params, admin_params);
  m_states.emplace(&index, FinalizationState(*GetGenesisState(), FinalizationState::COMPLETED));
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

FinalizationState *Storage::Set(const CBlockIndex &block_index, FinalizationState &&state) {
  LOCK(cs);
  const auto res = m_states.emplace(&block_index, std::move(state));
  if (res.second) {
    return &res.first->second;
  }
  return nullptr;
}

bool Storage::Confirm(const CBlockIndex &block_index, FinalizationState &&new_state, FinalizationState **state_out) {
  assert(new_state.GetInitStatus() == esperanza::FinalizationState::COMPLETED);

  LOCK(cs);
  const auto it = m_states.find(&block_index);
  assert(it != m_states.end());
  const auto &old_state = it->second;
  assert(old_state.GetInitStatus() == esperanza::FinalizationState::FROM_COMMITS);
  bool result = old_state == new_state;

  m_states.erase(it);
  const auto res = m_states.emplace(&block_index, std::move(new_state));
  assert(res.second);
  if (state_out != nullptr) {
    *state_out = &res.first->second;
  }
  return result;
}

class StateStorageImpl final : public StateStorage {
 public:
  explicit StateStorageImpl(Dependency<staking::ActiveChain> active_chain)
      : m_active_chain(active_chain) {}

  esperanza::FinalizationState *GetState() override;
  esperanza::FinalizationState *GetState(const CBlockIndex &block_index) override;
  const esperanza::FinalizationParams &GetFinalizationParams() const override;
  const esperanza::AdminParams &GetAdminParams() const override;

  bool ProcessNewCommits(const CBlockIndex &block_index, const std::vector<CTransactionRef> &txes) override;
  bool ProcessNewTipCandidate(const CBlockIndex &block_index, const CBlock &block) override;
  bool ProcessNewTip(const CBlockIndex &block_index, const CBlock &block) override;
  void RestoreFromDisk(const CChainParams &chainparams) override;
  void Reset(const esperanza::FinalizationParams &params,
             const esperanza::AdminParams &admin_params) override;
  void ResetToTip(const CBlockIndex &block_index) override;

 private:
  bool ProcessNewTipWorker(const CBlockIndex &block_index, const CBlock &block);
  bool FinalizationHappened(const CBlockIndex &block_index, blockchain::Height *out_height);
  void Trim(blockchain::Height height);

  Dependency<staking::ActiveChain> m_active_chain;
  Storage m_storage;

  // TODO: these members is configured via Reset(). It's done to keep a way how
  // FinalizationState::Init and FinalizationState::Reset worked. Let's remove Reset
  // function and configure component once via constructor.
  const esperanza::FinalizationParams *m_finalization_params = nullptr;
  const esperanza::AdminParams *m_admin_params = nullptr;
};

bool StateStorageImpl::ProcessNewTipWorker(const CBlockIndex &block_index, const CBlock &block) {
  const auto state = m_storage.FindOrCreate(block_index, FinalizationState::COMPLETED);
  if (state == nullptr) {
    LogPrint(BCLog::FINALIZATION, "ERROR: Cannot find or create finalization state for %s\n",
             block_index.GetBlockHash().GetHex());
    return false;
  }

  switch (state->GetInitStatus()) {
    case FinalizationState::NEW: {
      return state->ProcessNewTip(block_index, block);
    }

    case FinalizationState::FROM_COMMITS: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s heigh=%d has been processed from commits, confirming...\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      assert(block_index.pprev != nullptr);  // we don't process commits of genesis block
      const auto ancestor_state = m_storage.Find(*block_index.pprev);
      assert(ancestor_state != nullptr);
      FinalizationState new_state(*ancestor_state);
      if (!new_state.ProcessNewTip(block_index, block)) {
        return false;
      }
      if (m_storage.Confirm(block_index, std::move(new_state), nullptr)) {
        // UNIT-E TODO: DoS commits sender.
        LogPrint(BCLog::FINALIZATION, "WARN: After processing the block_hash=%s height=%d, its finalization state differs from one given from commits. Overwrite it anyway.\n",
                 block_index.GetBlockHash().GetHex(), block_index.nHeight);
      } else {
        LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d confirmed\n",
                 block_index.GetBlockHash().GetHex(), block_index.nHeight);
      }
      return true;
    }

    case FinalizationState::COMPLETED: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      return true;
    }
  }

  assert(not("unreachable"));  // suppress gcc warning
}

bool StateStorageImpl::FinalizationHappened(const CBlockIndex &block_index, blockchain::Height *out_height) {
  if (block_index.pprev == nullptr) {
    return false;
  }
  const auto *prev_state = GetState(*block_index.pprev);
  const auto *new_state = GetState(block_index);
  if (prev_state == nullptr || new_state == nullptr) {
    return false;
  }

  const auto epoch_length = GetFinalizationParams().epoch_length;
  // workaround first epoch finalization
  if (static_cast<blockchain::Height>(block_index.nHeight) == epoch_length) {
    if (out_height != nullptr) {
      *out_height = epoch_length - 1;
    }
    return true;
  }

  const auto prev_fin_epoch = prev_state->GetLastFinalizedEpoch();
  const auto new_fin_epoch = new_state->GetLastFinalizedEpoch();
  if (prev_fin_epoch == new_fin_epoch) {
    return false;
  }

  assert(new_fin_epoch > prev_fin_epoch);
  if (out_height != nullptr) {
    *out_height = (new_fin_epoch + 1) * epoch_length - 1;
  }
  return true;
}

void StateStorageImpl::Trim(blockchain::Height height) {
  LogPrint(BCLog::FINALIZATION, "Trimming finalization storage for height < %d\n", height);
  m_storage.ClearUntilHeight(height);
}

FinalizationState *StateStorageImpl::GetState() {
  const auto index = m_active_chain->GetTip();
  if (index == nullptr) {
    return nullptr;
  }
  return GetState(*index);
}

FinalizationState *StateStorageImpl::GetState(const CBlockIndex &block_index) {
  return m_storage.Find(block_index);
}

const esperanza::FinalizationParams &StateStorageImpl::GetFinalizationParams() const {
  assert(m_finalization_params != nullptr);
  return *m_finalization_params;
}

const esperanza::AdminParams &StateStorageImpl::GetAdminParams() const {
  assert(m_admin_params != nullptr);
  return *m_admin_params;
}

bool StateStorageImpl::ProcessNewTip(const CBlockIndex &block_index, const CBlock &block) {
  LogPrint(BCLog::FINALIZATION, "Process tip block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);
  if (!ProcessNewTipWorker(block_index, block)) {
    return false;
  }
  if (block_index.nHeight > 0 && !m_storage.Restoring() &&
      (block_index.nHeight + 2) % m_storage.GetGenesisState()->GetEpochLength() == 0) {
    // Generate the snapshot for the block which is one block behind the last one.
    // The last epoch block will contain the snapshot hash pointing to this snapshot.
    snapshot::Creator::GenerateOrSkip(GetState()->GetCurrentEpoch());
  }
  blockchain::Height finalization_height = 0;
  if (FinalizationHappened(block_index, &finalization_height)) {
    Trim(finalization_height);
  }
  return true;
}

bool StateStorageImpl::ProcessNewTipCandidate(const CBlockIndex &block_index, const CBlock &block) {
  LogPrint(BCLog::FINALIZATION, "Process candidate tip block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);
  return ProcessNewTipWorker(block_index, block);
}

bool StateStorageImpl::ProcessNewCommits(const CBlockIndex &block_index, const std::vector<CTransactionRef> &txes) {
  LogPrint(BCLog::FINALIZATION, "Process commits block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);
  const auto state = m_storage.FindOrCreate(block_index, FinalizationState::FROM_COMMITS);
  if (state == nullptr) {
    LogPrint(BCLog::FINALIZATION, "ERROR: Cannot find or create finalization state for %s\n",
             block_index.GetBlockHash().GetHex());
    return false;
  }

  switch (state->GetInitStatus()) {
    case esperanza::FinalizationState::NEW: {
      return state->ProcessNewCommits(block_index, txes);
    }

    case esperanza::FinalizationState::FROM_COMMITS: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed from commits\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      return true;
    }

    case esperanza::FinalizationState::COMPLETED: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      return true;
    }
  }

  assert(not("unreachable"));  // suppress gcc warning
}

// In this version we read all the blocks from the disk.
// This function might be significantly optimized by using finalization
// state serialization.
void StateStorageImpl::RestoreFromDisk(const CChainParams &chainparams) {
  Storage::RestoringRAII restoring(m_storage);
  if (fPruneMode) {
    const auto tip = m_active_chain->GetTip();
    if (tip != nullptr) {
      m_storage.ResetToTip(chainparams.GetFinalization(),
                           chainparams.GetAdminParams(),
                           *tip);
    } else {
      m_storage.Reset(chainparams.GetFinalization(), chainparams.GetAdminParams());
    }
    return;
  }

  LogPrint(BCLog::FINALIZATION, "Restore finalization state from disk\n");
  m_storage.Reset(chainparams.GetFinalization(), chainparams.GetAdminParams());
  for (blockchain::Height i = 1; i <= m_active_chain->GetHeight(); ++i) {
    const CBlockIndex *const index = m_active_chain->AtHeight(i);
    CBlock block;
    if (!ReadBlockFromDisk(block, index, chainparams.GetConsensus())) {
      assert(not("Failed to read block"));
    }
    const bool ok = ProcessNewTip(*index, block);
    assert(ok);
  }
}

void StateStorageImpl::Reset(const esperanza::FinalizationParams &params,
                             const esperanza::AdminParams &admin_params) {
  m_finalization_params = &params;
  m_admin_params = &admin_params;
  m_storage.Reset(params, admin_params);
}

void StateStorageImpl::ResetToTip(const CBlockIndex &block_index) {
  m_storage.ResetToTip(GetFinalizationParams(), GetAdminParams(), block_index);
}

}  // namespace

std::unique_ptr<StateStorage> StateStorage::New(Dependency<staking::ActiveChain> active_chain) {
  return MakeUnique<StateStorageImpl>(active_chain);
}

}  // namespace finalization
