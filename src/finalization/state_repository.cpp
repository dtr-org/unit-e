// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/state_repository.h>

#include <blockdb.h>
#include <esperanza/finalizationstate.h>
#include <finalization/state_db.h>
#include <finalization/state_processor.h>
#include <staking/active_chain.h>
#include <staking/block_index_map.h>
#include <validation.h>

namespace finalization {
namespace {

class RepositoryImpl final : public StateRepository {
 public:
  explicit RepositoryImpl(
      Dependency<staking::BlockIndexMap> block_index_map,
      Dependency<staking::ActiveChain> active_chain,
      Dependency<finalization::StateDB> state_db,
      Dependency<BlockDB> block_db)
      : m_block_index_map(block_index_map),
        m_active_chain(active_chain),
        m_state_db(state_db),
        m_block_db(block_db) {}

  FinalizationState *GetTipState() override;
  FinalizationState *Find(const CBlockIndex &block_index) override;
  FinalizationState *FindOrCreate(const CBlockIndex &block_index,
                                  FinalizationState::InitStatus required_parent_status) override;
  bool Confirm(const CBlockIndex &block_index,
               FinalizationState &&new_state,
               FinalizationState **state_out) override;

  bool RestoreFromDisk(Dependency<finalization::StateProcessor> proc) override;
  bool Restoring() const override;
  bool SaveToDisk() override;

  void Reset(const esperanza::FinalizationParams &params,
             const esperanza::AdminParams &admin_params) override;
  void ResetToTip(const CBlockIndex &block_index) override;

  void TrimUntilHeight(blockchain::Height height) override;

  const esperanza::FinalizationParams &GetFinalizationParams() const override;
  const esperanza::AdminParams &GetAdminParams() const override;

 private:
  FinalizationState *Create(const CBlockIndex &block_index, FinalizationState::InitStatus required_parent_status);
  bool ProcessNewTipWorker(const CBlockIndex &block_index, const CBlock &block);
  bool FinalizationHappened(const CBlockIndex &block_index);
  FinalizationState *GetGenesisState() const;
  bool LoadStatesFromDB();
  const FinalizationState *FindBestState();
  void CheckAndRecover(Dependency<finalization::StateProcessor> proc);

  Dependency<staking::BlockIndexMap> m_block_index_map;
  Dependency<staking::ActiveChain> m_active_chain;
  Dependency<finalization::StateDB> m_state_db;
  Dependency<BlockDB> m_block_db;

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
  LOCK(cs);
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
  LOCK(cs);
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
  LOCK(cs);
  if (const auto state = Find(block_index)) {
    return state;
  }
  return Create(block_index, required_parent_status);
}

void RepositoryImpl::Reset(const esperanza::FinalizationParams &params,
                           const esperanza::AdminParams &admin_params) {
  LOCK(cs);
  LogPrint(BCLog::FINALIZATION, "Completely reset state repository\n");
  m_states.clear();
  m_genesis_state.reset(new FinalizationState(params, admin_params));
  m_finalization_params = &params;
  m_admin_params = &admin_params;
}

void RepositoryImpl::ResetToTip(const CBlockIndex &block_index) {
  LOCK(cs);
  LogPrint(BCLog::FINALIZATION, "Reset state repository to the tip=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);
  m_states.clear();
  m_states.emplace(&block_index, FinalizationState(*GetGenesisState(), FinalizationState::COMPLETED));
}

void RepositoryImpl::TrimUntilHeight(blockchain::Height height) {
  LOCK(cs);
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
  LOCK(cs);
  return m_genesis_state.get();
}

bool RepositoryImpl::Confirm(const CBlockIndex &block_index,
                             FinalizationState &&new_state,
                             FinalizationState **state_out) {
  assert(new_state.GetInitStatus() == esperanza::FinalizationState::COMPLETED);

  LOCK(cs);

  const auto it = m_states.find(&block_index);
  assert(it != m_states.end());
  const auto &old_state = it->second;
  assert(old_state.GetInitStatus() == esperanza::FinalizationState::FROM_COMMITS);
  const bool result = old_state == new_state;

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

bool RepositoryImpl::RestoreFromDisk(Dependency<finalization::StateProcessor> proc) {
  LOCK(cs);
  RestoringRAII restoring(*this);
  if (!LoadStatesFromDB()) {
    return error("States restoring failed\n");
  }
  LogPrint(BCLog::FINALIZATION, "Loaded %d states\n", m_states.size());
  CheckAndRecover(proc);
  LogPrint(BCLog::FINALIZATION, "States after recovering: %d\n", m_states.size());
  return true;
}

bool RepositoryImpl::LoadStatesFromDB() {
  const boost::optional<uint32_t> last_finalized_epoch =
      m_state_db->FindLastFinalizedEpoch(GetFinalizationParams(), GetAdminParams());

  if (last_finalized_epoch) {
    if (*last_finalized_epoch > 0) {
      LogPrint(BCLog::FINALIZATION, "Restoring state repository from disk, last_finalized_epoch=%d\n",
               *last_finalized_epoch);
      const blockchain::Height height =
          GetFinalizationParams().GetEpochCheckpointHeight(*last_finalized_epoch + 1);
      m_state_db->LoadStatesHigherThan(height, GetFinalizationParams(), GetAdminParams(), &m_states);
      if (!m_states.empty()) {
        return true;
      }
      LogPrint(BCLog::FINALIZATION, "WARN: 0 states loaded, fallback to full load\n");
    }
  }

  LogPrint(BCLog::FINALIZATION, "Restore state repository from disk, Load all states.\n");
  if (!m_state_db->Load(GetFinalizationParams(), GetAdminParams(), &m_states)) {
    return false;
  }

  return true;
}

void RepositoryImpl::CheckAndRecover(Dependency<finalization::StateProcessor> proc) {

  AssertLockHeld(cs);

  const FinalizationState *state = FindBestState();
  if (state == nullptr) {
    return;
  }

  const uint32_t last_finalized_epoch = state->GetLastFinalizedEpoch();

  const blockchain::Height height = last_finalized_epoch == 0 ? 0 : GetFinalizationParams().GetEpochCheckpointHeight(last_finalized_epoch + 1);

  m_block_index_map->ForEach([this, height, proc](const uint256 &, const CBlockIndex &index) {
    const CBlockIndex *origin = m_active_chain->FindForkOrigin(index);
    if (origin == nullptr || static_cast<blockchain::Height>(origin->nHeight) <= height) {
      return true;
    }
    std::list<const CBlockIndex *> missed;
    FinalizationState *state = nullptr;
    const CBlockIndex *walk = &index;
    while (walk != nullptr && state == nullptr) {
      state = Find(*walk);
      if (state != nullptr) {
        break;
      }
      if (m_state_db->Load(*walk, GetFinalizationParams(), GetAdminParams(), &m_states)) {
        state = Find(*walk);
        assert(state != nullptr);
        break;
      }
      missed.push_front(walk);
      walk = walk->pprev;
    }
    if (!missed.empty()) {
      LogPrintf("WARN: State for block=%s height=%d missed in the finalization state database.\n", index.GetBlockHash().GetHex(), index.nHeight);
      LogPrintf("Trying to recover the following states from block index database or block files: %s\n",
                util::to_string([&missed] {
                                  std::vector<std::string> r;
                                  r.reserve(missed.size());
                                  for (auto const &m : missed) {
                                    r.emplace_back(m->GetBlockHash().GetHex());
                                  }
                                  return r; }()));
    }
    while (!missed.empty()) {
      const CBlockIndex *index = missed.front();
      const CBlockIndex *target = missed.back();
      missed.pop_front();
      // UNITE TODO: Uncomment once we can trust commits
      // (commits merkle root added to the header and FROM_COMMITS is dropped).
      // Check #836 for details.
      //
      // if (index->commits) {
      //   proc->ProcessNewCommits(*index, *index->commits);
      //   continue;
      // }
      boost::optional<CBlock> block = m_block_db->ReadBlock(*index);
      if (!block) {
        LogPrintf("Cannot read block=%s to restore finalization state for block=%s.\n",
                  index->GetBlockHash().GetHex(), target->GetBlockHash().GetHex());
        LogPrintf("Need sync\n");
        throw MissedBlockError(*index);
      }
      const bool ok = proc->ProcessNewTipCandidate(*index, *block);
      assert(ok);
    }
    return true;
  });

  TrimUntilHeight(height);
}

const FinalizationState *RepositoryImpl::FindBestState() {
  AssertLockHeld(m_active_chain->GetLock());

  const CBlockIndex *walk = m_active_chain->GetTip();
  while (walk != nullptr) {
    if (const auto *state = Find(*walk)) {
      return state;
    }
    walk = walk->pprev;
  }

  return nullptr;
}

bool RepositoryImpl::SaveToDisk() {
  LOCK(cs);
  LogPrint(BCLog::FINALIZATION, "Flushing %s finalization states to the disk\n", m_states.size());
  return m_state_db->Save(m_states);
}

bool RepositoryImpl::Restoring() const {
  return m_restoring;
}

}  // namespace

std::unique_ptr<StateRepository> StateRepository::New(
    Dependency<staking::BlockIndexMap> block_index_map,
    Dependency<staking::ActiveChain> active_chain,
    Dependency<finalization::StateDB> state_db,
    Dependency<BlockDB> block_db) {

  return MakeUnique<RepositoryImpl>(block_index_map, active_chain, state_db, block_db);
}

}  // namespace finalization
