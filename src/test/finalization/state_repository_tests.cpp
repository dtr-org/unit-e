// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/adminparams.h>
#include <esperanza/finalizationstate.h>
#include <finalization/state_processor.h>
#include <finalization/state_repository.h>
#include <test/test_unite.h>
#include <test/test_unite_mocks.h>

#include <boost/test/unit_test.hpp>

namespace esperanza {
static std::ostream &operator<<(std::ostream &os, const FinalizationState &f) {
  os << f.ToString();
  return os;
}
}  // namespace esperanza

namespace {

class BlockDBMock : public BlockDB {
 public:
  boost::optional<CBlock> ReadBlock(const CBlockIndex &index) override {
    const auto it = blocks.find(&index);
    if (it == blocks.end()) {
      return boost::none;
    }
    ++it->second.read_requests;
    return it->second.block;
  }

  struct Info {
    size_t read_requests = 0;
    CBlock block;
  };
  std::map<const CBlockIndex *, Info> blocks;
};

class StateDBMock : public finalization::StateDB {
  using FinalizationState = finalization::FinalizationState;

 public:
  std::map<const CBlockIndex *, FinalizationState> m_states;
  boost::optional<uint32_t> m_last_finalized_epoch;

  bool Save(const std::map<const CBlockIndex *, FinalizationState> &states) override {
    for (const auto &s : states) {
      m_states.emplace(s.first, finalization::FinalizationState(s.second, s.second.GetInitStatus()));
    }
    return true;
  }

  bool Load(std::map<const CBlockIndex *, FinalizationState> *states) override {
    for (const auto &s : m_states) {
      states->emplace(s.first, finalization::FinalizationState(s.second, s.second.GetInitStatus()));
    }
    return true;
  }

  bool Load(const CBlockIndex &index,
            std::map<const CBlockIndex *, FinalizationState> *states) const override {
    const auto it = m_states.find(&index);
    if (it == m_states.end()) {
      return false;
    }
    states->emplace(it->first, finalization::FinalizationState(it->second, it->second.GetInitStatus()));
    return true;
  }

  bool Erase(const CBlockIndex &index) override {
    return m_states.erase(&index) > 0;
  }

  boost::optional<uint32_t> FindLastFinalizedEpoch() const override {
    return m_last_finalized_epoch;
  }

  void LoadStatesHigherThan(
      blockchain::Height height,
      std::map<const CBlockIndex *, FinalizationState> *states) const override {}

  FinalizationState &Get(const CBlockIndex &index) {
    const auto it = m_states.find(&index);
    BOOST_REQUIRE(it != m_states.end());
    return it->second;
  }
};

class Fixture {
 public:
  Fixture() : m_repo(NewRepo()) {
    m_chain.mock_AtHeight.SetStub([this](blockchain::Height h) -> CBlockIndex * {
      auto const it = this->m_block_heights.find(h);
      if (it == this->m_block_heights.end()) {
        return nullptr;
      }
      return it->second;
    });
  }

  std::unique_ptr<finalization::StateRepository> NewRepo() {
    return finalization::StateRepository::New(
        &m_finalization_params, &m_block_indexes, &m_chain, &m_state_db, &m_block_db);
  }

  CBlockIndex &CreateBlockIndex() {
    const auto height = FindNextHeight();
    CBlockIndex &index = *m_block_indexes.Insert(uint256S(std::to_string(height)));
    index.nHeight = height;
    index.pprev = m_block_heights[height - 1];
    index.nStatus |= BLOCK_HAVE_DATA;
    m_chain.mock_GetTip.SetResult(&index);
    m_chain.mock_GetHeight.SetResult(height);
    m_block_heights[index.nHeight] = &index;
    return index;
  }

  finalization::Params m_finalization_params;
  std::unique_ptr<finalization::StateRepository> m_repo;
  mocks::ActiveChainFake m_chain;
  StateDBMock m_state_db;
  BlockDBMock m_block_db;
  mocks::BlockIndexMapFake m_block_indexes;

 private:
  blockchain::Height FindNextHeight() {
    if (m_chain.GetTip() == nullptr) {
      return 0;
    } else {
      return m_chain.GetTip()->nHeight + 1;
    }
  }

  std::map<blockchain::Height, CBlockIndex *> m_block_heights;  // m_block_index owns these block indexes
};

}  // namespace

BOOST_FIXTURE_TEST_SUITE(state_repository_tests, BasicTestingSetup)

using S = esperanza::FinalizationState::InitStatus;

BOOST_AUTO_TEST_CASE(basic_checks) {
  Fixture fixture;
  const auto &b0 = fixture.CreateBlockIndex();
  const auto &b1 = fixture.CreateBlockIndex();
  const auto &b2 = fixture.CreateBlockIndex();
  const auto &b3 = fixture.CreateBlockIndex();
  const auto &b4 = fixture.CreateBlockIndex();

  finalization::StateRepository &repo = *fixture.m_repo;
  auto &stored_states = fixture.m_state_db.m_states;

  LOCK(repo.GetLock());

  BOOST_CHECK(repo.Find(b0) != nullptr);  // we have a state for genesis block
  BOOST_CHECK(repo.Find(b1) == nullptr);
  BOOST_CHECK(repo.Find(b2) == nullptr);

  // Create a new state.
  auto *state1 = repo.FindOrCreate(b1, S::COMPLETED);
  BOOST_REQUIRE(state1 != nullptr);
  BOOST_CHECK(state1->GetInitStatus() == S::NEW);
  BOOST_CHECK(repo.Find(b1) == state1);
  BOOST_CHECK(repo.FindOrCreate(b1, S::COMPLETED) == state1);

  // Try to create a state for a second block. It must fail due to parent's state is NEW.
  BOOST_CHECK(repo.FindOrCreate(b2, S::COMPLETED) == nullptr);
  BOOST_CHECK(repo.FindOrCreate(b2, S::FROM_COMMITS) == nullptr);

  // Now relax requirement for parent's state, so that we can create new state
  auto *state2 = repo.FindOrCreate(b2, S::NEW);
  BOOST_REQUIRE(state1 != nullptr);
  BOOST_CHECK(state1->GetInitStatus() == S::NEW);

  // Try to create a state when repository doesn't contain parent's state
  BOOST_CHECK(repo.FindOrCreate(b4, S::COMPLETED) == nullptr);
  BOOST_CHECK(repo.FindOrCreate(b4, S::FROM_COMMITS) == nullptr);
  BOOST_CHECK(repo.FindOrCreate(b4, S::NEW) == nullptr);

  // Process state2 from commits and create state for b3.
  state2->ProcessNewCommits(b2, {});
  BOOST_CHECK(state2->GetInitStatus() == S::FROM_COMMITS);
  BOOST_CHECK(repo.FindOrCreate(b3, S::COMPLETED) == nullptr);
  auto *state3 = repo.FindOrCreate(b3, S::FROM_COMMITS);
  BOOST_REQUIRE(state3 != nullptr);
  state3->ProcessNewCommits(b3, {});
  BOOST_CHECK(state3->GetInitStatus() == S::FROM_COMMITS);

  // Check we cannot create next state with COMPLETED requirement.
  BOOST_CHECK(repo.FindOrCreate(b4, S::COMPLETED) == nullptr);

  // Now, confirm the state3
  esperanza::FinalizationState state3_confirmed(*state2);
  state3_confirmed.ProcessNewTip(b3, CBlock());
  BOOST_CHECK(repo.Confirm(b3, std::move(state3_confirmed), &state3));
  BOOST_CHECK(state3->GetInitStatus() == S::COMPLETED);
  BOOST_CHECK(repo.Find(b3) == state3);

  // Now we can create next state with COMPLETED requirement.
  auto *state4 = repo.FindOrCreate(b4, S::COMPLETED);
  BOOST_REQUIRE(state4 != nullptr);
  BOOST_CHECK(state4->GetInitStatus() == S::NEW);

  // Check state_db integration
  repo.SaveToDisk();
  BOOST_CHECK_EQUAL(stored_states.count(&b0), 0);  // we don't store genesis state on disk
  BOOST_CHECK_EQUAL(stored_states.count(&b1), 1);
  BOOST_CHECK_EQUAL(stored_states.count(&b2), 1);
  BOOST_CHECK_EQUAL(stored_states.count(&b3), 1);
  BOOST_CHECK_EQUAL(stored_states.count(&b4), 1);

  // Trim the repository
  repo.TrimUntilHeight(3);
  BOOST_CHECK(repo.Find(b0) != nullptr);  // genesis
  BOOST_CHECK(repo.Find(b1) == nullptr);
  BOOST_CHECK(repo.Find(b2) == nullptr);
  BOOST_CHECK(repo.Find(b3) != nullptr);
  BOOST_CHECK(repo.Find(b4) != nullptr);
  BOOST_CHECK_EQUAL(stored_states.count(&b0), 0);  // we don't store genesis state on disk
  BOOST_CHECK_EQUAL(stored_states.count(&b1), 0);
  BOOST_CHECK_EQUAL(stored_states.count(&b2), 0);
  BOOST_CHECK_EQUAL(stored_states.count(&b3), 1);
  BOOST_CHECK_EQUAL(stored_states.count(&b4), 1);

  // Btw, now we processed states til the chain's tip. Check it.
  BOOST_CHECK(repo.GetTipState() == state4);
}

BOOST_AUTO_TEST_CASE(recovering) {
  Fixture fixture;

  LOCK(fixture.m_chain.GetLock());
  LOCK(fixture.m_repo->GetLock());

  auto check_restored = [&fixture](finalization::StateRepository &restored) {
    for (blockchain::Height i = 0; i <= fixture.m_chain.GetHeight(); ++i) {
      const CBlockIndex *index = fixture.m_chain.AtHeight(i);
      BOOST_REQUIRE(index != nullptr);
      finalization::FinalizationState *state = fixture.m_repo->Find(*index);
      finalization::FinalizationState *restored_state = restored.Find(*index);
      BOOST_REQUIRE(state != nullptr);
      BOOST_REQUIRE(restored_state != nullptr);
      BOOST_CHECK_EQUAL(*state, *restored_state);
    }
  };

  auto remove_from_db = [&fixture](const blockchain::Height h) {
    const CBlockIndex *index = fixture.m_chain.AtHeight(h);
    BOOST_REQUIRE(index != nullptr);
    BOOST_REQUIRE(fixture.m_state_db.m_states.count(index) == 1);
    fixture.m_state_db.m_states.erase(index);
    BOOST_REQUIRE(fixture.m_state_db.m_states.count(index) == 0);
  };

  for (size_t i = 0; i < 5; ++i) {
    const CBlockIndex &index = fixture.CreateBlockIndex();
    finalization::FinalizationState *state = fixture.m_repo->FindOrCreate(index, S::NEW);
    BOOST_REQUIRE(state != nullptr);
    state->ProcessNewTip(index, CBlock());
  }

  fixture.m_repo->SaveToDisk();

  for (const CBlockIndex *walk = fixture.m_chain.GetTip(); walk != nullptr && walk->nHeight > 0; walk = walk->pprev) {
    finalization::FinalizationState *state = fixture.m_repo->Find(*walk);
    BOOST_REQUIRE(state != nullptr);
    BOOST_CHECK_EQUAL(fixture.m_state_db.Get(*walk), *state);
  }

  // Check normal scenario
  {
    auto restored_repo = fixture.NewRepo();
    auto proc = finalization::StateProcessor::New(
        &fixture.m_finalization_params, restored_repo.get(), &fixture.m_chain);
    restored_repo->RestoreFromDisk(proc.get());
    LOCK(restored_repo->GetLock());
    check_restored(*restored_repo);
  }

  // Remove one state from DB and check how it's restored.
  {
    remove_from_db(2);
    fixture.m_block_db.blocks.clear();
    fixture.m_block_db.blocks[fixture.m_chain.AtHeight(2)] = {};
    auto restored_repo = fixture.NewRepo();
    auto proc = finalization::StateProcessor::New(
        &fixture.m_finalization_params, restored_repo.get(), &fixture.m_chain);
    restored_repo->RestoreFromDisk(proc.get());
    BOOST_CHECK_EQUAL(fixture.m_block_db.blocks[fixture.m_chain.AtHeight(2)].read_requests, 1);
    LOCK(restored_repo->GetLock());
    BOOST_CHECK(restored_repo->Find(*fixture.m_chain.AtHeight(2)) != nullptr);
    check_restored(*restored_repo);
  }

  // Remove second state from DB and check how it's restored.
  {
    remove_from_db(3);
    fixture.m_block_db.blocks.clear();
    fixture.m_block_db.blocks[fixture.m_chain.AtHeight(2)] = {};
    fixture.m_block_db.blocks[fixture.m_chain.AtHeight(3)] = {};
    auto restored_repo = fixture.NewRepo();
    auto proc = finalization::StateProcessor::New(
        &fixture.m_finalization_params, restored_repo.get(), &fixture.m_chain);
    restored_repo->RestoreFromDisk(proc.get());
    BOOST_CHECK_EQUAL(fixture.m_block_db.blocks[fixture.m_chain.AtHeight(2)].read_requests, 1);
    BOOST_CHECK_EQUAL(fixture.m_block_db.blocks[fixture.m_chain.AtHeight(3)].read_requests, 1);
    LOCK(restored_repo->GetLock());
    BOOST_CHECK(restored_repo->Find(*fixture.m_chain.AtHeight(2)) != nullptr);
    BOOST_CHECK(restored_repo->Find(*fixture.m_chain.AtHeight(3)) != nullptr);
    check_restored(*restored_repo);
  }

  // Remove tip's state from DB and check how it's restored.
  {
    remove_from_db(4);
    fixture.m_block_db.blocks.clear();
    fixture.m_block_db.blocks[fixture.m_chain.AtHeight(2)] = {};
    fixture.m_block_db.blocks[fixture.m_chain.AtHeight(3)] = {};
    fixture.m_block_db.blocks[fixture.m_chain.AtHeight(4)] = {};
    auto restored_repo = fixture.NewRepo();
    auto proc = finalization::StateProcessor::New(
        &fixture.m_finalization_params, restored_repo.get(), &fixture.m_chain);
    restored_repo->RestoreFromDisk(proc.get());
    BOOST_CHECK_EQUAL(fixture.m_block_db.blocks[fixture.m_chain.AtHeight(2)].read_requests, 1);
    BOOST_CHECK_EQUAL(fixture.m_block_db.blocks[fixture.m_chain.AtHeight(3)].read_requests, 1);
    BOOST_CHECK_EQUAL(fixture.m_block_db.blocks[fixture.m_chain.AtHeight(4)].read_requests, 1);
    LOCK(restored_repo->GetLock());
    BOOST_CHECK(restored_repo->Find(*fixture.m_chain.AtHeight(2)) != nullptr);
    BOOST_CHECK(restored_repo->Find(*fixture.m_chain.AtHeight(3)) != nullptr);
    BOOST_CHECK(restored_repo->Find(*fixture.m_chain.AtHeight(4)) != nullptr);
    check_restored(*restored_repo);
  }

  // Remove tip's state from DB and check how it's restored (backward ordering in BlockIndexMap.ForEach).
  {
    fixture.m_block_db.blocks.clear();
    fixture.m_block_db.blocks[fixture.m_chain.AtHeight(2)] = {};
    fixture.m_block_db.blocks[fixture.m_chain.AtHeight(3)] = {};
    fixture.m_block_db.blocks[fixture.m_chain.AtHeight(4)] = {};
    fixture.m_block_indexes.reverse = true;
    auto restored_repo = fixture.NewRepo();
    auto proc = finalization::StateProcessor::New(
        &fixture.m_finalization_params, restored_repo.get(), &fixture.m_chain);
    restored_repo->RestoreFromDisk(proc.get());
    BOOST_CHECK_EQUAL(fixture.m_block_db.blocks[fixture.m_chain.AtHeight(2)].read_requests, 1);
    BOOST_CHECK_EQUAL(fixture.m_block_db.blocks[fixture.m_chain.AtHeight(3)].read_requests, 1);
    BOOST_CHECK_EQUAL(fixture.m_block_db.blocks[fixture.m_chain.AtHeight(4)].read_requests, 1);
    LOCK(restored_repo->GetLock());
    BOOST_CHECK(restored_repo->Find(*fixture.m_chain.AtHeight(2)) != nullptr);
    BOOST_CHECK(restored_repo->Find(*fixture.m_chain.AtHeight(3)) != nullptr);
    BOOST_CHECK(restored_repo->Find(*fixture.m_chain.AtHeight(4)) != nullptr);
    check_restored(*restored_repo);
  }

  // Remove block 3 from the disk!
  {
    fixture.m_block_db.blocks.clear();
    fixture.m_block_db.blocks[fixture.m_chain.AtHeight(2)] = {};
    fixture.m_block_db.blocks[fixture.m_chain.AtHeight(4)] = {};
    auto restored_repo = fixture.NewRepo();
    auto proc = finalization::StateProcessor::New(
        &fixture.m_finalization_params, restored_repo.get(), &fixture.m_chain);
    try {
      restored_repo->RestoreFromDisk(proc.get());
    } catch (finalization::MissedBlockError &e) {
      BOOST_CHECK(&e.missed_index == fixture.m_chain.AtHeight(3));
      return;
    }
    BOOST_REQUIRE(not("unreachable"));
  }

  // Remove one state from DB and check how it's restored from CBlockIndex
  {
    remove_from_db(4);
    fixture.m_block_db.blocks.clear();
    auto restored_repo = fixture.NewRepo();
    auto proc = finalization::StateProcessor::New(
        &fixture.m_finalization_params, restored_repo.get(), &fixture.m_chain);
    std::vector<CTransactionRef> commits;
    fixture.m_chain.AtHeight(5)->commits = commits;
    restored_repo->RestoreFromDisk(proc.get());
    BOOST_CHECK(fixture.m_block_db.blocks.empty());
    LOCK(restored_repo->GetLock());
    BOOST_CHECK(restored_repo->Find(*fixture.m_chain.AtHeight(4)) != nullptr);
    check_restored(*restored_repo);
  }

  // Cannot recover finalization state for new tip
  {
    CBlockIndex &index = fixture.CreateBlockIndex();
    index.nStatus &= ~BLOCK_HAVE_DATA;
    auto restored_repo = fixture.NewRepo();
    auto proc = finalization::StateProcessor::New(
        &fixture.m_finalization_params, restored_repo.get(), &fixture.m_chain);
    BOOST_CHECK_THROW(restored_repo->RestoreFromDisk(proc.get()), std::runtime_error);
  }

  // Move tip one block back. Repository must try to recover it but won't throw as it's not
  // on the main chain.
  {
    const CBlockIndex *tip = fixture.m_chain.GetTip();
    fixture.m_chain.mock_GetTip.SetResult(tip->pprev);
    auto restored_repo = fixture.NewRepo();
    auto proc = finalization::StateProcessor::New(
        &fixture.m_finalization_params, restored_repo.get(), &fixture.m_chain);
    restored_repo->RestoreFromDisk(proc.get());
    check_restored(*restored_repo);
  }
}

BOOST_AUTO_TEST_SUITE_END()
