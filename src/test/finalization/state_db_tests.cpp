// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/state_db.h>

#include <finalization/params.h>

#include <test/esperanza/finalizationstate_utils.h>
#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
#include <test/util/blocktools.h>
#include <boost/test/unit_test.hpp>

namespace esperanza {
static std::ostream &operator<<(std::ostream &os, const FinalizationState &f) {
  os << f.ToString();
  return os;
}
}  // namespace esperanza

namespace util {
template <>
std::string stringify(const CBlockIndex *const &index) {
  if (index == nullptr) {
    return "null";
  }
  return stringify<uint256>(index->GetBlockHash());
}
}  // namespace util

BOOST_AUTO_TEST_SUITE(state_db_tests)

BOOST_AUTO_TEST_CASE(leveldb_rand) {
  mocks::ActiveChainFake active_chain;
  mocks::BlockIndexMapFake block_index_map;
  Settings settings;
  finalization::StateDBParams params;
  params.inmemory = true;

  finalization::Params finalization_params;
  std::unique_ptr<finalization::StateDB> db = finalization::StateDB::NewFromParams(
      params, &settings, &finalization_params, &block_index_map, &active_chain);

  LOCK(block_index_map.GetLock());

  std::map<const CBlockIndex *, esperanza::FinalizationState> original;
  for (size_t i = 0; i < 100; ++i) {
    CBlockIndex *block_index = block_index_map.Insert(GetRandHash());
    FinalizationStateSpy state(finalization_params);
    state.shuffle();
    original.emplace(block_index, std::move(state));
  }
  BOOST_REQUIRE_EQUAL(original.size(), 100);

  db->Save(original);

  std::map<const CBlockIndex *, esperanza::FinalizationState> restored;
  bool const result = db->Load(&restored);

  BOOST_CHECK(result);
  BOOST_CHECK_EQUAL(restored, original);
}

BOOST_AUTO_TEST_CASE(erase_states) {
  mocks::ActiveChainFake active_chain;
  mocks::BlockIndexMapFake block_index_map;
  Settings settings;
  finalization::StateDBParams params;
  params.inmemory = true;

  finalization::Params finalization_params;
  std::unique_ptr<finalization::StateDB> db = finalization::StateDB::NewFromParams(
      params, &settings, &finalization_params, &block_index_map, &active_chain);

  LOCK(block_index_map.GetLock());

  std::map<const CBlockIndex *, esperanza::FinalizationState> original;
  for (size_t i = 0; i < 100; ++i) {
    CBlockIndex *block_index = block_index_map.Insert(GetRandHash());
    FinalizationStateSpy state(finalization_params);
    state.shuffle();
    original.emplace(block_index, std::move(state));
  }
  BOOST_REQUIRE_EQUAL(original.size(), 100);

  db->Save(original);

  // Erase ~1/3 of states, check availability
  std::set<const CBlockIndex *> erased_states;
  std::set<const CBlockIndex *> alive_states;
  for (const auto &kv : original) {
    if (GetRand(3) == 0) {
      db->Erase(*kv.first);
      erased_states.emplace(kv.first);
    } else {
      alive_states.emplace(kv.first);
    }
  }
  std::map<const CBlockIndex *, esperanza::FinalizationState> restored;
  for (const CBlockIndex *index : erased_states) {
    BOOST_CHECK_EQUAL(db->Load(*index, &restored), false);
  }
  for (const CBlockIndex *index : alive_states) {
    BOOST_CHECK_EQUAL(db->Load(*index, &restored), true);
  }
}

BOOST_AUTO_TEST_CASE(load_best_states) {
  Settings settings;
  finalization::StateDBParams params;
  params.inmemory = true;
  finalization::Params finalization_params;

  mocks::ActiveChainMock active_chain;
  blocktools::BlockIndexFake block_index_fake;
  mocks::BlockIndexMapMock block_index_map;

  block_index_map.mock_ForEach.SetStub([&](std::function<bool(const uint256 &, const CBlockIndex &)> &&f) {
    for (auto &entry : block_index_fake.block_indexes) {
      const CBlockIndex &block_index = entry.second;
      f(block_index.GetBlockHash(), block_index);
    }
  });
  block_index_map.mock_Lookup.SetStub([&](const uint256 &block_hash) -> CBlockIndex * {
    auto result = block_index_fake.block_indexes.find(block_hash);
    if (result == block_index_fake.block_indexes.end()) {
      return nullptr;
    }
    return &result->second;
  });

  std::unique_ptr<finalization::StateDB> db = finalization::StateDB::NewFromParams(
      params, &settings, &finalization_params, &block_index_map, &active_chain);

  LOCK(block_index_map.GetLock());
  LOCK(active_chain.GetLock());

  std::map<const CBlockIndex *, esperanza::FinalizationState> original;

  // Generate an active chain with 100 blocks
  {
    const CBlockIndex *b99 = block_index_fake.Generate(100);
    const std::vector<CBlockIndex *> chain = block_index_fake.GetChain(b99);
    for (CBlockIndex *block_index : chain) {
      FinalizationStateSpy state(finalization_params);
      state.shuffle();
      original.emplace(block_index, std::move(state));
    }
    block_index_fake.SetupActiveChain(b99, active_chain);
    BOOST_REQUIRE_EQUAL(original.size(), 100);
    BOOST_REQUIRE_EQUAL(active_chain.GetTip()->nHeight, 99);
  }

  // Generate fork 1 branching off at height=50
  {
    const CBlockIndex *b50 = active_chain.AtHeight(50);
    const CBlockIndex *tip_fork1 = block_index_fake.Generate(100, b50);
    const std::vector<CBlockIndex *> chain = block_index_fake.GetChain(tip_fork1);
    for (CBlockIndex *block_index : chain) {
      FinalizationStateSpy state(finalization_params);
      state.shuffle();
      original.emplace(block_index, std::move(state));
    }
    BOOST_REQUIRE_EQUAL(original.size(), 200);
    BOOST_REQUIRE_EQUAL(active_chain.GetTip()->nHeight, 99);
  }

  // Generate fork 2 branching off at height=80
  {
    const CBlockIndex *b80 = active_chain.AtHeight(80);
    const CBlockIndex *tip_fork2 = block_index_fake.Generate(100, b80);
    const std::vector<CBlockIndex *> chain = block_index_fake.GetChain(tip_fork2);
    for (CBlockIndex *block_index : chain) {
      FinalizationStateSpy state(finalization_params);
      state.shuffle();
      original.emplace(block_index, std::move(state));
    }
    BOOST_REQUIRE_EQUAL(original.size(), 300);
    BOOST_REQUIRE_EQUAL(active_chain.GetTip()->nHeight, 99);
  }

  db->Save(original);

  const auto it = original.find(active_chain.GetTip());
  BOOST_REQUIRE(it != original.end());
  const uint32_t expected_last_finalized_epoch = it->second.GetLastFinalizedEpoch();

  // Find last finalized epoch
  {
    boost::optional<uint32_t> last_finalized_epoch = db->FindLastFinalizedEpoch();
    BOOST_CHECK(static_cast<bool>(last_finalized_epoch));
    BOOST_CHECK_EQUAL(*last_finalized_epoch, expected_last_finalized_epoch);
  }

  // Simulate that node cannot load state for tip, then it must load most recent state
  // accoring to the main chain. Simply move active chain forward but keep db as is.
  {
    const CBlockIndex *b104 = block_index_fake.Generate(5, active_chain.GetTip());
    block_index_fake.SetupActiveChain(b104, active_chain);
    BOOST_REQUIRE(active_chain.GetTip()->nHeight == 104);
  }

  // Check that db can find last finalized epoch
  {
    boost::optional<uint32_t> last_finalized_epoch = db->FindLastFinalizedEpoch();
    BOOST_CHECK(static_cast<bool>(last_finalized_epoch));
    BOOST_CHECK_EQUAL(*last_finalized_epoch, expected_last_finalized_epoch);
  }

  // Load states from height 60.
  // States for active chain starting from 60 must be loaded (40 items).
  // States for fork 2 must be loaded (100 items).
  // States for fork 1 must be ignored.
  {
    std::map<const CBlockIndex *, esperanza::FinalizationState> restored;
    db->LoadStatesHigherThan(59, &restored);
    BOOST_CHECK_EQUAL(restored.size(), 140);

    for (auto &kv : restored) {
      const CBlockIndex *index = kv.first;
      const esperanza::FinalizationState &state = kv.second;
      BOOST_CHECK(index->nHeight >= 60);

      const auto oit = original.find(index);
      BOOST_REQUIRE(oit != original.end());

      BOOST_CHECK_EQUAL(state, oit->second);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
