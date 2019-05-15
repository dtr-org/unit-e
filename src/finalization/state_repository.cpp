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
      const Dependency<finalization::Params> finalization_params,
      const Dependency<staking::BlockIndexMap> block_index_map,
      const Dependency<staking::ActiveChain> active_chain,
      const Dependency<finalization::StateDB> state_db,
      const Dependency<BlockDB> block_db)
      : m_finalization_params(finalization_params),
        m_block_index_map(block_index_map),
        m_active_chain(active_chain),
        m_state_db(state_db),
        m_block_db(block_db),
        m_genesis_state(new FinalizationState(*m_finalization_params)) {}

  CCriticalSection &GetLock() override { return m_cs; }
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

  void TrimUntilHeight(blockchain::Height height) override;

 private:
  FinalizationState *Create(const CBlockIndex &block_index, FinalizationState::InitStatus required_parent_status);
  bool ProcessNewTipWorker(const CBlockIndex &block_index, const CBlock &block);
  bool FinalizationHappened(const CBlockIndex &block_index);
  FinalizationState *GetGenesisState() const;
  bool LoadStatesFromDB();
  const FinalizationState *FindBestState();
  void CheckAndRecover(Dependency<finalization::StateProcessor> proc);

  const Dependency<finalization::Params> m_finalization_params;
  const Dependency<staking::BlockIndexMap> m_block_index_map;
  const Dependency<staking::ActiveChain> m_active_chain;
  const Dependency<finalization::StateDB> m_state_db;
  const Dependency<BlockDB> m_block_db;

  mutable CCriticalSection m_cs;
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
  AssertLockHeld(m_cs);
  const auto *block_index = m_active_chain->GetTip();
  if (block_index == nullptr) {
    return nullptr;
  }
  return Find(*block_index);
}

FinalizationState *RepositoryImpl::Find(const CBlockIndex &block_index) {
  AssertLockHeld(m_cs);
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
  AssertLockHeld(m_cs);
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
  AssertLockHeld(m_cs);
  if (const auto state = Find(block_index)) {
    return state;
  }
  return Create(block_index, required_parent_status);
}

void RepositoryImpl::TrimUntilHeight(blockchain::Height height) {
  LOCK(m_cs);
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
  AssertLockHeld(m_cs);
  return m_genesis_state.get();
}

bool RepositoryImpl::Confirm(const CBlockIndex &block_index,
                             FinalizationState &&new_state,
                             FinalizationState **state_out) {
  assert(new_state.GetInitStatus() == esperanza::FinalizationState::COMPLETED);

  AssertLockHeld(m_cs);
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

bool RepositoryImpl::RestoreFromDisk(Dependency<finalization::StateProcessor> proc) {
  LOCK(m_cs);
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
  AssertLockHeld(m_cs);

  const boost::optional<uint32_t> last_finalized_epoch =
      m_state_db->FindLastFinalizedEpoch();

  if (last_finalized_epoch) {
    if (*last_finalized_epoch > 0) {
      LogPrint(BCLog::FINALIZATION, "Restoring state repository from disk, last_finalized_epoch=%d\n",
               *last_finalized_epoch);
      const blockchain::Height height =
          m_finalization_params->GetEpochCheckpointHeight(*last_finalized_epoch);
      m_state_db->LoadStatesHigherThan(height, &m_states);
      if (!m_states.empty()) {
        return true;
      }
      LogPrint(BCLog::FINALIZATION, "WARN: 0 states loaded, fallback to full load\n");
    }
  }

  LogPrint(BCLog::FINALIZATION, "Restore state repository from disk, Load all states.\n");
  if (!m_state_db->Load(&m_states)) {
    return false;
  }

  return true;
}

void RepositoryImpl::CheckAndRecover(Dependency<finalization::StateProcessor> proc) {
  AssertLockHeld(m_cs);

  const FinalizationState *state = FindBestState();
  if (state == nullptr) {
    return;
  }

  const uint32_t last_finalized_epoch = state->GetLastFinalizedEpoch();

  const blockchain::Height height = m_finalization_params->GetEpochCheckpointHeight(last_finalized_epoch);

  std::set<const CBlockIndex *> unrecoverable;

  m_block_index_map->ForEach([this, height, proc, &unrecoverable](const uint256 &, const CBlockIndex &index) {
    // This index has already been checked and marked as unrecoverable
    if (unrecoverable.count(&index) != 0) {
      return true;
    }
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
      if (m_state_db->Load(*walk, &m_states)) {
        state = Find(*walk);
        assert(state != nullptr);
        break;
      }
      missed.push_front(walk);
      walk = walk->pprev;
    }
    while (!missed.empty()) {
      const CBlockIndex *index = missed.front();
      const CBlockIndex *target = missed.back();
      missed.pop_front();
      if (index->commits) {
        if (proc->ProcessNewCommits(*index, *index->commits)) {
          LogPrintf("Finalization state for block=%s height=%d has been recovered from block index\n",
                    index->GetBlockHash().GetHex(), index->nHeight);
          continue;
        }
      }
      if (index->nStatus & BLOCK_HAVE_DATA) {
        boost::optional<CBlock> block = m_block_db->ReadBlock(*index);
        if (!block) {
          LogPrintf("Cannot read block=%s to restore finalization state for block=%s.\n",
                    index->GetBlockHash().GetHex(), target->GetBlockHash().GetHex());
          LogPrintf("Need sync\n");
          throw MissedBlockError(*index);
        }
        if (proc->ProcessNewTipCandidate(*index, *block)) {
          LogPrintf("Finalization state for block=%s height=%d has been recovered from block\n",
                    index->GetBlockHash().GetHex(), index->nHeight);
        }
        continue;
      }
      unrecoverable.emplace(index);
    }
    return true;
  });

  for (const auto *u : unrecoverable) {
    if (m_active_chain->Contains(*u)) {
      LogPrintf("Cannot recover finalization state for block=%s height=%d\n",
                u->GetBlockHash().GetHex(), u->nHeight);
      throw std::runtime_error("Need sync or reindex");
    }
  }

  if (!unrecoverable.empty()) {
    LogPrintf("%d finalization states have not been recovered, but it seems to be safe to continue.\n",
              unrecoverable.size());
  }

  TrimUntilHeight(height);
}

const FinalizationState *RepositoryImpl::FindBestState() {
  AssertLockHeld(m_active_chain->GetLock());
  AssertLockHeld(m_cs);

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
  LOCK(m_cs);
  LogPrint(BCLog::FINALIZATION, "Flushing %s finalization states to the disk\n", m_states.size());
  return m_state_db->Save(m_states);
}

bool RepositoryImpl::Restoring() const {
  return m_restoring;
}

}  // namespace

std::unique_ptr<StateRepository> StateRepository::New(
    Dependency<finalization::Params> finalization_params,
    Dependency<staking::BlockIndexMap> block_index_map,
    Dependency<staking::ActiveChain> active_chain,
    Dependency<finalization::StateDB> state_db,
    Dependency<BlockDB> block_db) {

  return MakeUnique<RepositoryImpl>(
      finalization_params, block_index_map, active_chain, state_db, block_db);
}

}  // namespace finalization
