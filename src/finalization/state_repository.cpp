// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/state_repository.h>

#include <chainparams.h>
#include <esperanza/finalizationstate.h>
#include <finalization/state_processor.h>
#include <validation.h>

namespace finalization {
namespace {

class RepositoryImpl final : public StateRepository {
 public:
  explicit RepositoryImpl(Dependency<staking::ActiveChain> active_chain)
      : m_active_chain(active_chain) {}

  CCriticalSection &GetLock() const override;

  FinalizationState *GetTipState() override;
  FinalizationState *Find(const CBlockIndex &block_index) override;
  FinalizationState *FindOrCreate(const CBlockIndex &block_index,
                                  FinalizationState::InitStatus required_parent_status) override;
  bool Confirm(const CBlockIndex &block_index,
               FinalizationState &&new_state,
               FinalizationState **state_out) override;

  void RestoreFromDisk(const CChainParams &chainparams,
                       Dependency<finalization::StateProcessor> proc) override;
  bool Restoring() const override;
  void Reset(const esperanza::FinalizationParams &params,
             const esperanza::AdminParams &admin_params) override;
  void ResetToTip(const CBlockIndex &block_index) override;
  void TrimUntilHeight(blockchain::Height height) override;

  const esperanza::FinalizationParams &GetFinalizationParams() const override;
  const esperanza::AdminParams &GetAdminParams() const override;

 private:
  FinalizationState *Create(const CBlockIndex &block_index, FinalizationState::InitStatus required_parent_status);
  bool ProcessNewTipWorker(const CBlockIndex &block_index, const CBlock &block);
  bool FinalizationHappened(const CBlockIndex &block_index, blockchain::Height *out_height);
  FinalizationState *GetGenesisState() const;

  Dependency<staking::ActiveChain> m_active_chain;

  // UNIT-E TODO: these members is configured via Reset(). It's done to keep a way how
  // FinalizationState::Init and FinalizationState::Reset worked. Let's remove Reset
  // function and configure component once via constructor.
  const esperanza::FinalizationParams *m_finalization_params = nullptr;
  const esperanza::AdminParams *m_admin_params = nullptr;

  mutable CCriticalSection cs;
  std::map<const CBlockIndex *, FinalizationState> m_states;
  std::unique_ptr<FinalizationState> m_genesis_state;
  std::atomic<bool> m_restoring{false};

  struct RestoringRAII {
    RepositoryImpl &r;
    explicit RestoringRAII(RepositoryImpl &r) : r(r) { r.m_restoring = true; }
    ~RestoringRAII() { r.m_restoring = false; }
  };
};

FinalizationState *RepositoryImpl::GetTipState() {
  const auto *block_index = m_active_chain->GetTip();
  if (block_index == nullptr) {
    return nullptr;
  }
  return Find(*block_index);
}

FinalizationState *RepositoryImpl::Find(const CBlockIndex &block_index) {
  AssertLockHeld(cs);
  if (block_index.nHeight == 0) {
    return GetGenesisState();
  }
  const auto it = m_states.find(&block_index);
  if (it == m_states.end()) {
    return nullptr;
  }
  return &it->second;
}

FinalizationState *RepositoryImpl::Create(const CBlockIndex &block_index,
                                          FinalizationState::InitStatus required_parent_status) {
  AssertLockHeld(cs);
  if (block_index.pprev == nullptr) {
    return nullptr;
  }

  const auto parent_state = Find(*block_index.pprev);
  if ((parent_state == nullptr) ||
      (parent_state != GetGenesisState() && parent_state->GetInitStatus() < required_parent_status)) {
    return nullptr;
  }

  const auto res = m_states.emplace(&block_index, FinalizationState(*parent_state));
  return &res.first->second;
}

FinalizationState *RepositoryImpl::FindOrCreate(const CBlockIndex &block_index,
                                                FinalizationState::InitStatus required_parent_status) {
  AssertLockHeld(cs);
  if (const auto state = Find(block_index)) {
    return state;
  }
  return Create(block_index, required_parent_status);
}

void RepositoryImpl::Reset(const esperanza::FinalizationParams &params,
                           const esperanza::AdminParams &admin_params) {
  LogPrint(BCLog::FINALIZATION, "Completely reset state repository\n");
  AssertLockHeld(cs);
  m_states.clear();
  m_genesis_state.reset(new FinalizationState(params, admin_params));
  m_finalization_params = &params;
  m_admin_params = &admin_params;
}

void RepositoryImpl::ResetToTip(const CBlockIndex &block_index) {
  LOCK(cs);
  Reset(*m_finalization_params, *m_admin_params);
  m_states.emplace(&block_index, FinalizationState(*GetGenesisState(), FinalizationState::COMPLETED));
}

void RepositoryImpl::TrimUntilHeight(blockchain::Height height) {
  AssertLockHeld(cs);
  LogPrint(BCLog::FINALIZATION, "Trimming state repository for height < %d\n", height);
  for (auto it = m_states.begin(); it != m_states.end();) {
    const CBlockIndex *index = it->first;
    if (!m_active_chain->Contains(*index)) {
      index = m_active_chain->FindForkOrigin(*index);
      assert(index != nullptr);
    }
    if (static_cast<blockchain::Height>(index->nHeight) < height) {
      it = m_states.erase(it);
    } else {
      ++it;
    }
  }
}

FinalizationState *RepositoryImpl::GetGenesisState() const {
  AssertLockHeld(cs);
  return m_genesis_state.get();
}

bool RepositoryImpl::Confirm(const CBlockIndex &block_index,
                             FinalizationState &&new_state,
                             FinalizationState **state_out) {
  AssertLockHeld(cs);

  assert(new_state.GetInitStatus() == esperanza::FinalizationState::COMPLETED);

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

const esperanza::FinalizationParams &RepositoryImpl::GetFinalizationParams() const {
  assert(m_finalization_params != nullptr);
  return *m_finalization_params;
}

const esperanza::AdminParams &RepositoryImpl::GetAdminParams() const {
  assert(m_admin_params != nullptr);
  return *m_admin_params;
}

// In this version we read all the blocks from the disk.
// This function might be significantly optimized by using finalization
// state serialization. Until then we have to have a processor dependecy here.
void RepositoryImpl::RestoreFromDisk(const CChainParams &chainparams,
                                     Dependency<finalization::StateProcessor> proc) {
  RestoringRAII restoring(*this);
  if (fPruneMode) {
    const auto tip = m_active_chain->GetTip();
    if (tip != nullptr) {
      ResetToTip(*tip);
    } else {
      Reset(chainparams.GetFinalization(), chainparams.GetAdminParams());
    }
    return;
  }

  LogPrint(BCLog::FINALIZATION, "Restore state repository from disk\n");
  Reset(chainparams.GetFinalization(), chainparams.GetAdminParams());
  for (blockchain::Height i = 1; i <= m_active_chain->GetHeight(); ++i) {
    const CBlockIndex *const index = m_active_chain->AtHeight(i);
    CBlock block;
    if (!ReadBlockFromDisk(block, index, chainparams.GetConsensus())) {
      assert(not("Failed to read block"));
    }
    const bool ok = proc->ProcessNewTip(*index, block);
    assert(ok);
  }
}

bool RepositoryImpl::Restoring() const {
  return m_restoring;
}

CCriticalSection &RepositoryImpl::GetLock() const {
  return cs;
}

}  // namespace

std::unique_ptr<StateRepository> StateRepository::New(Dependency<staking::ActiveChain> active_chain) {
  return MakeUnique<RepositoryImpl>(active_chain);
}

}  // namespace finalization
