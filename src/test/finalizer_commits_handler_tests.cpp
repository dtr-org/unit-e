// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <boost/test/unit_test.hpp>
#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
#include <test/esperanza/finalizationstate_utils.h>

#include <finalization/state_processor.h>
#include <finalization/state_repository.h>
#include <p2p/finalizer_commits_handler_impl.h>

template <typename Os>
Os &operator<<(Os &os, const CBlockIndex *index) {
  if (index == nullptr) {
    os << "NULL";
  } else {
    os << index->GetBlockHash();
  }
  return os;
}

namespace {

using FinalizationState = finalization::FinalizationState;

class FinalizerCommitsHandlerSpy : public p2p::FinalizerCommitsHandlerImpl {
public:
  template<typename... Args>
  FinalizerCommitsHandlerSpy(Args&&... args)
    : p2p::FinalizerCommitsHandlerImpl(std::forward<Args>(args)...) {}

  using FinalizerCommitsHandlerImpl::FindMostRecentStart;
  using FinalizerCommitsHandlerImpl::FindStop;
};

class RepoMock : public finalization::StateRepository {
public:
  RepoMock(const esperanza::FinalizationParams &params) : m_params(params), state(m_params) { }

  FinalizationState *GetTipState() override { return &state; }
  FinalizationState *Find(const CBlockIndex &) override { return &state; }
  FinalizationState *FindOrCreate(const CBlockIndex &, FinalizationState::InitStatus) override { return &state; }
  bool Confirm(const CBlockIndex &, FinalizationState &&, FinalizationState **) override { return false; }
  void RestoreFromDisk(const CChainParams &, Dependency<finalization::StateProcessor>) override { }
  bool Restoring() const override { return false; }
  void ResetToTip(const CBlockIndex &) override { }
  void TrimUntilHeight(const blockchain::Height) override { }
  const esperanza::FinalizationParams &GetFinalizationParams() const override { return m_params; }
  const esperanza::AdminParams &GetAdminParams() const override { return m_admin_params; }
  void Reset(const esperanza::FinalizationParams &, const esperanza::AdminParams &) override { }

  FinalizationStateSpy state;

private:
  esperanza::FinalizationParams m_params;
  esperanza::AdminParams m_admin_params;
};

class Fixture {
 public:
  static constexpr blockchain::Height epoch_length = 5;

  static const esperanza::FinalizationParams GetFinalizationParams() {
    auto params = Params().GetFinalization();
    params.epoch_length = epoch_length;
    return params;
  }

  Fixture()
    : repo(GetFinalizationParams()),
      commits(&active_chain, &repo, /*finalization::StateProcessor*/ nullptr) {

    active_chain.block_at_height = [this](blockchain::Height h) -> CBlockIndex * {
      auto const it = this->m_block_heights.find(h);
      if (it == this->m_block_heights.end()) {
        return nullptr;
      }
      return it->second;
    };
    active_chain.find_fork_origin = [this](const CBlockIndex *index) -> const CBlockIndex * {
      while (index != nullptr && !active_chain.Contains(*index)) {
        index = index->pprev;
      }
      return index;
    };
    active_chain.get_block_index = [this](const uint256 &hash) -> CBlockIndex * {
      const auto it = m_block_indexes.find(hash);
      if (it == m_block_indexes.end()) {
        return nullptr;
      }
      return &it->second;
    };
  }

  CBlockIndex &CreateBlockIndex() {
    const auto height = FindNextHeight();
    const auto ins_res = m_block_indexes.emplace(uint256S(std::to_string(height)), CBlockIndex());
    CBlockIndex &index = ins_res.first->second;
    index.nHeight = height;
    index.phashBlock = &ins_res.first->first;
    index.pprev = active_chain.tip;
    active_chain.tip = &index;
    m_block_heights[index.nHeight] = &index;
    if (height == 0) {
      active_chain.genesis = &index;
    }
    return index;
  }

  void AddBlocks(const size_t amount) {
    for (size_t i = 0; i < amount; ++i) {
      CreateBlockIndex();
    }
  }

  mocks::ActiveChainMock active_chain;
  RepoMock repo;
  FinalizerCommitsHandlerSpy commits;

 private:
  blockchain::Height FindNextHeight() {
    if (active_chain.tip == nullptr) {
      return 0;
    } else {
      return active_chain.GetTip()->nHeight + 1;
    }
  }

  std::map<uint256, CBlockIndex> m_block_indexes;
  std::map<blockchain::Height, CBlockIndex *> m_block_heights; // m_block_index owns these block indexes
};

}

BOOST_FIXTURE_TEST_SUITE(finalizer_commits_handler_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(find_most_recent_start) {
  Fixture fixture;

  staking::ActiveChain &chain = fixture.active_chain;
  FinalizationStateSpy &state = fixture.repo.state;
  FinalizerCommitsHandlerSpy &commits = fixture.commits;

  using Locator = p2p::FinalizerCommitsLocator;

  LOCK(chain.GetLock());

  fixture.AddBlocks(5);
  BOOST_REQUIRE(state.GetEpochLength() == 5);
  BOOST_REQUIRE(state.GetLastFinalizedEpoch() == 0);

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(0)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(0));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(3)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullptr);
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(0)->GetBlockHash(),
          chain.AtHeight(3)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(3));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(0)->GetBlockHash(),
          chain.AtHeight(2)->GetBlockHash(),
          chain.AtHeight(1)->GetBlockHash(),
          chain.AtHeight(3)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(2));
  }

  state.SetLastFinalizedEpoch(1);
  fixture.AddBlocks(16);
  state.SetLastFinalizedEpoch(2); // block 14
  BOOST_REQUIRE(state.GetLastFinalizedEpoch() == 2);

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(13)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullptr);
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(9)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(9));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(9)->GetBlockHash(),
          chain.AtHeight(14)->GetBlockHash(),
          chain.AtHeight(19)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(19));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(9)->GetBlockHash(),
          chain.AtHeight(14)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(14));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(9)->GetBlockHash(),
          chain.AtHeight(19)->GetBlockHash(),
          chain.AtHeight(14)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(19));
  }

  std::map<blockchain::Height, uint256> fork_hashes;
  std::map<blockchain::Height, CBlockIndex> fork;

  CBlockIndex *prev = const_cast<CBlockIndex*>(chain.AtHeight(15));
  for (blockchain::Height h = 16; h < 21; ++h) {
    fork_hashes[h] = uint256S(std::to_string(1000 + h));
    fork[h].phashBlock = &fork_hashes[h];
    fork[h].nHeight = h;
    fork[h].pprev = prev;
    prev = &fork[h];
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(14)->GetBlockHash(),
          fork[20].GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(14));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(14)->GetBlockHash(),
          fork[19].GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(14));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          fork[19].GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullptr);
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(4)->GetBlockHash(),
          fork[19].GetBlockHash(),
          chain.AtHeight(14)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(4));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(19)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullptr);
  }

  state.SetLastFinalizedEpoch(3); // block 19
  BOOST_REQUIRE(state.GetLastFinalizedEpoch() == 3);

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(19)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(19));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(18)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullptr);
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(20)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullptr);
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(19)->GetBlockHash(),
          chain.AtHeight(20)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(20));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          fork[19].GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullptr);
  }
}

BOOST_AUTO_TEST_CASE(find_stop) {
  Fixture fixture;

  staking::ActiveChain &chain = fixture.active_chain;
  FinalizerCommitsHandlerSpy &commits = fixture.commits;

  using Locator = p2p::FinalizerCommitsLocator;

  LOCK(chain.GetLock());

  fixture.AddBlocks(5);

  {
    const CBlockIndex *result = commits.FindStop(Locator{
        {}, uint256()});
    BOOST_CHECK_EQUAL(result, nullptr);
  }

  {
    const CBlockIndex *result = commits.FindStop(Locator{
        {}, uint256S("12345")});
    BOOST_CHECK_EQUAL(result, nullptr);
  }

  for (blockchain::Height h = 1; h < 5; ++h) {
    const CBlockIndex *result = commits.FindStop(Locator{
        {}, chain.AtHeight(h)->GetBlockHash()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(h));
  }
}

BOOST_AUTO_TEST_SUITE_END()
