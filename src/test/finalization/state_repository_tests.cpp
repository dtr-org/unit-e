// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/adminparams.h>
#include <esperanza/finalizationstate.h>
#include <finalization/state_repository.h>
#include <test/test_unite.h>
#include <test/test_unite_mocks.h>

#include <boost/test/unit_test.hpp>

namespace {

class Fixture {
 public:
  Fixture() : repo(finalization::StateRepository::New(&m_chain)) {
    Reset();
  }

  void Reset() {
    repo->Reset(Params().GetFinalization(), Params().GetAdminParams());
    m_chain.block_at_height = [this](blockchain::Height h) -> CBlockIndex * {
      auto const it = this->m_block_heights.find(h);
      if (it == this->m_block_heights.end()) {
        return nullptr;
      }
      return it->second;
    };
    m_chain.find_fork_origin = [this](const CBlockIndex *index) {
      while (index != nullptr && !m_chain.Contains(*index)) {
        index = index->pprev;
      }
      return index;
    };
  }

  CBlockIndex &CreateBlockIndex() {
    const auto height = FindNextHeight();
    const auto ins_res = m_block_indexes.emplace(uint256S(std::to_string(height)), CBlockIndex());
    CBlockIndex &index = ins_res.first->second;
    index.nHeight = height;
    index.phashBlock = &ins_res.first->first;
    index.pprev = m_chain.tip;
    m_chain.tip = &index;
    m_block_heights[index.nHeight] = &index;
    return index;
  }

  std::unique_ptr<finalization::StateRepository> repo;

 private:
  blockchain::Height FindNextHeight() {
    if (m_chain.tip == nullptr) {
      return 0;
    } else {
      return m_chain.GetTip()->nHeight + 1;
    }
  }

  mocks::ActiveChainMock m_chain;
  std::map<uint256, CBlockIndex> m_block_indexes;
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

  BOOST_CHECK(fixture.repo->Find(b0) != nullptr);  // we have a state for genesis block
  BOOST_CHECK(fixture.repo->Find(b1) == nullptr);
  BOOST_CHECK(fixture.repo->Find(b2) == nullptr);

  // Create a new state.
  auto *state1 = fixture.repo->FindOrCreate(b1, S::COMPLETED);
  BOOST_REQUIRE(state1 != nullptr);
  BOOST_CHECK(state1->GetInitStatus() == S::NEW);
  BOOST_CHECK(fixture.repo->Find(b1) == state1);
  BOOST_CHECK(fixture.repo->FindOrCreate(b1, S::COMPLETED) == state1);

  // Try to create a state for a second block. It must fail due to parent's state is NEW.
  BOOST_CHECK(fixture.repo->FindOrCreate(b2, S::COMPLETED) == nullptr);
  BOOST_CHECK(fixture.repo->FindOrCreate(b2, S::FROM_COMMITS) == nullptr);

  // Now relax requirement for parent's state, so that we can create new state
  auto *state2 = fixture.repo->FindOrCreate(b2, S::NEW);
  BOOST_REQUIRE(state1 != nullptr);
  BOOST_CHECK(state1->GetInitStatus() == S::NEW);

  // Try to create a state when repository doesn't contain parent's state
  BOOST_CHECK(fixture.repo->FindOrCreate(b4, S::COMPLETED) == nullptr);
  BOOST_CHECK(fixture.repo->FindOrCreate(b4, S::FROM_COMMITS) == nullptr);
  BOOST_CHECK(fixture.repo->FindOrCreate(b4, S::NEW) == nullptr);

  // Process state2 from commits and create state for b3.
  state2->ProcessNewCommits(b2, {});
  BOOST_CHECK(state2->GetInitStatus() == S::FROM_COMMITS);
  BOOST_CHECK(fixture.repo->FindOrCreate(b3, S::COMPLETED) == nullptr);
  auto *state3 = fixture.repo->FindOrCreate(b3, S::FROM_COMMITS);
  BOOST_REQUIRE(state3 != nullptr);
  state3->ProcessNewCommits(b3, {});
  BOOST_CHECK(state3->GetInitStatus() == S::FROM_COMMITS);

  // Check we cannot create next state with COMPLETED requirement.
  BOOST_CHECK(fixture.repo->FindOrCreate(b4, S::COMPLETED) == nullptr);

  // Now, confirm the state3
  esperanza::FinalizationState state3_confirmed(*state2);
  state3_confirmed.ProcessNewTip(b3, CBlock());
  BOOST_CHECK(fixture.repo->Confirm(b3, std::move(state3_confirmed), &state3));
  BOOST_CHECK(state3->GetInitStatus() == S::COMPLETED);
  BOOST_CHECK(fixture.repo->Find(b3) == state3);

  // Now we can create next state with COMPLETED requirement.
  auto *state4 = fixture.repo->FindOrCreate(b4, S::COMPLETED);
  BOOST_REQUIRE(state4 != nullptr);
  BOOST_CHECK(state4->GetInitStatus() == S::NEW);

  // Trim the repository
  fixture.repo->TrimUntilHeight(3);
  BOOST_CHECK(fixture.repo->Find(b0) != nullptr);  // genesis
  BOOST_CHECK(fixture.repo->Find(b1) == nullptr);
  BOOST_CHECK(fixture.repo->Find(b2) == nullptr);
  BOOST_CHECK(fixture.repo->Find(b3) != nullptr);
  BOOST_CHECK(fixture.repo->Find(b4) != nullptr);

  // Btw, now we processed states til the chain's tip. Check it.
  BOOST_CHECK(fixture.repo->GetTipState() == state4);

  // Reset repo completely.
  fixture.Reset();
  BOOST_CHECK(fixture.repo->Find(b0) != nullptr);  // genesis
  BOOST_CHECK(fixture.repo->Find(b3) == nullptr);
  BOOST_CHECK(fixture.repo->Find(b4) == nullptr);

  // Reset  repo to the tip.
  fixture.repo->ResetToTip(b4);
  BOOST_CHECK(fixture.repo->Find(b3) == nullptr);
  state4 = fixture.repo->Find(b4);
  BOOST_REQUIRE(state4 != nullptr);
  BOOST_CHECK(state4->GetInitStatus() == S::COMPLETED);
}

BOOST_AUTO_TEST_SUITE_END()
