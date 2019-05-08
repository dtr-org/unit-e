// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/esperanza/finalizationstate_utils.h>
#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
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
  mocks::ActiveChainMock active_chain;
  mocks::BlockIndexMapMock block_index_map;
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
  BOOST_REQUIRE(original.size() == 100);

  db->Save(original);

  std::map<const CBlockIndex *, esperanza::FinalizationState> restored;
  bool const result = db->Load(&restored);

  BOOST_CHECK(result);
  BOOST_CHECK_EQUAL(restored, original);
}

class ActiveChainTest : public mocks::ActiveChainMock {
 public:
  ActiveChainTest() {
    this->stub_AtHeight = [this](blockchain::Height const h) -> CBlockIndex * {
      const auto it = m_block_heights.find(h);
      return it != m_block_heights.end() ? it->second : nullptr;
    };
  }

  void Add(CBlockIndex &index) {
    this->result_GetTip = &index;
    m_block_heights[index.nHeight] = &index;
  }

 private:
  std::map<blockchain::Height, CBlockIndex *> m_block_heights;
};

BOOST_AUTO_TEST_CASE(load_best_states) {
  ActiveChainTest active_chain;
  mocks::BlockIndexMapMock block_index_map;
  Settings settings;
  finalization::StateDBParams params;
  params.inmemory = true;
  finalization::Params finalization_params;

  std::unique_ptr<finalization::StateDB> db = finalization::StateDB::NewFromParams(
      params, &settings, &finalization_params, &block_index_map, &active_chain);

  LOCK(block_index_map.GetLock());
  LOCK(active_chain.GetLock());

  auto generate = [&](CBlockIndex *prev, bool add_to_chain) {
    const blockchain::Height h = prev != nullptr ? prev->nHeight + 1 : 0;
    CBlockIndex *index = block_index_map.Insert(GetRandHash());
    index->pprev = prev;
    index->nHeight = h;
    if (add_to_chain) {
      active_chain.Add(*index);
    }
    return index;
  };

  // Generate active chain
  std::map<const CBlockIndex *, esperanza::FinalizationState> original;
  for (size_t i = 0; i < 100; ++i) {
    CBlockIndex *block_index = generate(active_chain.result_GetTip, true);
    FinalizationStateSpy state(finalization_params);
    state.shuffle();
    original.emplace(block_index, std::move(state));
  }
  BOOST_REQUIRE(original.size() == 100);
  BOOST_REQUIRE(active_chain.GetTip()->nHeight == 99);

  // Generate fork 1 higher 50
  {
    CBlockIndex *index = active_chain.stub_AtHeight(50);
    for (size_t i = 0; i < 100; ++i) {
      index = generate(index, false);
      FinalizationStateSpy state(finalization_params);
      state.shuffle();
      original.emplace(index, std::move(state));
    }
  }
  BOOST_REQUIRE(original.size() == 200);
  BOOST_REQUIRE(active_chain.GetTip()->nHeight == 99);

  // Generate fork 2 higher 80
  {
    CBlockIndex *index = active_chain.stub_AtHeight(80);
    for (size_t i = 0; i < 100; ++i) {
      index = generate(index, false);
      FinalizationStateSpy state(finalization_params);
      state.shuffle();
      original.emplace(index, std::move(state));
    }
  }
  BOOST_REQUIRE(original.size() == 300);
  BOOST_REQUIRE(active_chain.GetTip()->nHeight == 99);

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

  for (size_t i = 0; i < 5; ++i) {
    generate(active_chain.result_GetTip, true);
  }
  BOOST_REQUIRE(active_chain.GetTip()->nHeight == 104);

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

    for (auto it = restored.begin(); it != restored.end(); ++it) {
      const CBlockIndex *index = it->first;
      const esperanza::FinalizationState &state = it->second;
      BOOST_CHECK(index->nHeight >= 60);

      const auto oit = original.find(index);
      BOOST_REQUIRE(oit != original.end());

      BOOST_CHECK_EQUAL(state, oit->second);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
