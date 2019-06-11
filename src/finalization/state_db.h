// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_FINALIZATION_STATE_DB_H
#define UNITE_FINALIZATION_STATE_DB_H

#include <blockchain/blockchain_types.h>
#include <dependency.h>
#include <settings.h>

namespace esperanza {
class FinalizationState;
struct FinalizationParams;
struct AdminParams;
}  // namespace esperanza

namespace staking {
class ActiveChain;
class BlockIndexMap;
}  // namespace staking

class CBlockIndex;
struct UnitEInjectorConfiguration;

namespace finalization {

using FinalizationState = esperanza::FinalizationState;
struct Params;

struct StateDBParams {
  size_t cache_size = 0;
  bool inmemory = false;
  bool wipe = false;
  bool obfuscate = false;
};

class StateDB {
 public:
  //! \brief Saves states.
  virtual bool Save(const std::map<const CBlockIndex *, FinalizationState> &states) = 0;

  //! \brief Loads all the states.
  virtual bool Load(std::map<const CBlockIndex *, FinalizationState> *states) = 0;

  //! \brief Loads specific state.
  virtual bool Load(const CBlockIndex &index,
                    std::map<const CBlockIndex *, FinalizationState> *states) const = 0;

  virtual bool Erase(const CBlockIndex &index) = 0;

  //! \brief Returns last finalized epoch accoring to active chain's tip.
  virtual boost::optional<uint32_t> FindLastFinalizedEpoch() const = 0;

  //! \brief Load most actual states.
  //!
  //! This function scans mapBlockIndex and consider to load finalization state if:
  //! * index is on main chain and higher than `height`.
  //! * index is a fork and it's origin is higher than `height`.
  virtual void LoadStatesHigherThan(
      blockchain::Height height,
      std::map<const CBlockIndex *, FinalizationState> *states) const = 0;

  virtual ~StateDB() = default;

  static std::unique_ptr<StateDB> New(
      Dependency<UnitEInjectorConfiguration>,
      Dependency<Settings>,
      Dependency<finalization::Params>,
      Dependency<staking::BlockIndexMap>,
      Dependency<staking::ActiveChain>,
      Dependency<ArgsManager>);

  static std::unique_ptr<StateDB> NewFromParams(
      const StateDBParams &,
      Dependency<Settings>,
      Dependency<finalization::Params>,
      Dependency<staking::BlockIndexMap>,
      Dependency<staking::ActiveChain>);
};

}  // namespace finalization

#endif  // UNITE_FINALIZATION_STATE_DB_H
