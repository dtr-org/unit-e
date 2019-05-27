// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/state_db.h>

#include <dbwrapper.h>
#include <esperanza/finalizationstate.h>
#include <finalization/params.h>
#include <injector_config.h>
#include <staking/active_chain.h>
#include <staking/block_index_map.h>
#include <validation.h>

namespace finalization {

namespace {

class StateDBImpl : public StateDB, public CDBWrapper {
 public:
  StateDBImpl(const StateDBParams &p,
              Dependency<Settings> settings,
              Dependency<finalization::Params> finalization_params,
              Dependency<staking::BlockIndexMap> block_index_map,
              Dependency<staking::ActiveChain> active_chain)
      : CDBWrapper(settings->data_dir / "finalization", p.cache_size, p.inmemory, p.wipe, p.obfuscate),
        m_finalization_params(finalization_params),
        m_block_index_map(block_index_map),
        m_active_chain(active_chain) {}

  bool Save(const std::map<const CBlockIndex *, FinalizationState> &states) override;

  //! \brief Loads all the states from leveldb.
  bool Load(std::map<const CBlockIndex *, FinalizationState> *states) override;

  //! \brief Loads specific state from leveldb.
  bool Load(const CBlockIndex &index,
            std::map<const CBlockIndex *, FinalizationState> *states) const override;

  //! \brief Returns last finalized epoch accoring to active chain's tip.
  boost::optional<uint32_t> FindLastFinalizedEpoch() const override;

  //! \brief Load most actual states from leveldb.
  //!
  //! This function scans mapBlockIndex and consider to load finalization state if:
  //! * index is on main chain and higher than `height`.
  //! * index is a fork and its origin is higher than `height`.
  void LoadStatesHigherThan(
      blockchain::Height height,
      std::map<const CBlockIndex *, FinalizationState> *states) const override;

 private:
  Dependency<finalization::Params> m_finalization_params;
  Dependency<staking::BlockIndexMap> m_block_index_map;
  Dependency<staking::ActiveChain> m_active_chain;
};

bool StateDBImpl::Save(const std::map<const CBlockIndex *, FinalizationState> &states) {
  CDBBatch batch(*this);
  for (const auto &i : states) {
    const uint256 &block_hash = i.first->GetBlockHash();
    const FinalizationState &state = i.second;
    batch.Write(block_hash, state);
  }
  return WriteBatch(batch, true);
}

bool StateDBImpl::Load(std::map<const CBlockIndex *, FinalizationState> *states) {

  assert(states != nullptr);
  AssertLockHeld(m_block_index_map->GetLock());

  states->clear();

  std::unique_ptr<CDBIterator> cursor(NewIterator());
  uint256 key;
  cursor->Seek(key);

  while (cursor->Valid()) {
    if (!cursor->GetKey(key)) {
      return error("%s: failed to get key", __func__);
    }
    const CBlockIndex *block_index = m_block_index_map->Lookup(key);
    if (block_index == nullptr) {
      return error("%s: failed to find block index %s", __func__, util::to_string(key));
    }
    FinalizationState state(*m_finalization_params);
    if (!cursor->GetValue(state)) {
      return error("%s: failed to get value for key %s", __func__, util::to_string(key));
    }
    const auto res = states->emplace(block_index, std::move(state));
    assert(res.second);
    cursor->Next();
  }
  return true;
}

bool StateDBImpl::Load(const CBlockIndex &index,
                       std::map<const CBlockIndex *, FinalizationState> *states) const {

  assert(states != nullptr);

  FinalizationState state(*m_finalization_params);
  if (Read(index.GetBlockHash(), state)) {
    states->emplace(&index, std::move(state));
    return true;
  }

  return false;
}

boost::optional<uint32_t> StateDBImpl::FindLastFinalizedEpoch() const {

  AssertLockHeld(m_active_chain->GetLock());

  const CBlockIndex *walk = m_active_chain->GetTip();

  while (walk != nullptr) {
    FinalizationState state(*m_finalization_params);
    if (Read(walk->GetBlockHash(), state)) {
      return state.GetLastFinalizedEpoch();
    }
    walk = walk->pprev;
  }

  return boost::none;
}

void StateDBImpl::LoadStatesHigherThan(
    const blockchain::Height height,
    std::map<const CBlockIndex *, FinalizationState> *states) const {

  assert(states != nullptr);
  AssertLockHeld(m_active_chain->GetLock());
  AssertLockHeld(m_block_index_map->GetLock());

  states->clear();

  m_block_index_map->ForEach([states, height, this](const uint256 &block_hash, const CBlockIndex &block_index) {
    const CBlockIndex *origin = m_active_chain->FindForkOrigin(block_index);
    if (origin != nullptr && static_cast<blockchain::Height>(origin->nHeight) > height) {
      FinalizationState state(*m_finalization_params);
      if (Read(block_hash, state)) {
        states->emplace(&block_index, std::move(state));
      }
    }
    return true;
  });
}

}  // namespace

std::unique_ptr<StateDB> StateDB::New(
    Dependency<UnitEInjectorConfiguration> config,
    Dependency<Settings> settings,
    Dependency<finalization::Params> finalization_params,
    Dependency<staking::BlockIndexMap> block_index_map,
    Dependency<staking::ActiveChain> active_chain,
    Dependency<ArgsManager> args_manager) {
  const bool reindex = args_manager->GetBoolArg("-reindex", false);
  StateDBParams state_db_params;
  if (reindex) {
    state_db_params.wipe = true;
  }
  state_db_params.inmemory = config->use_in_memory_databases;
  return NewFromParams(state_db_params, settings, finalization_params, block_index_map, active_chain);
}

std::unique_ptr<StateDB> StateDB::NewFromParams(
    const StateDBParams &params,
    Dependency<Settings> settings,
    Dependency<finalization::Params> finalization_params,
    Dependency<staking::BlockIndexMap> block_index_map,
    Dependency<staking::ActiveChain> active_chain) {
  if (!params.inmemory) {
    fs::create_directories(settings->data_dir);
  }
  return MakeUnique<StateDBImpl>(params, settings, finalization_params, block_index_map, active_chain);
}

}  // namespace finalization
