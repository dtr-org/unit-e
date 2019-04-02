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
  FinalizerCommitsHandlerSpy(Args&&... args) : p2p::FinalizerCommitsHandlerImpl(std::forward<Args>(args)...) {}

  using p2p::FinalizerCommitsHandlerImpl::FindMostRecentStart;
  using p2p::FinalizerCommitsHandlerImpl::FindStop;
  using p2p::FinalizerCommitsHandlerImpl::IsSameFork;
};

class Fixture {
 public:
  static constexpr blockchain::Height epoch_length = 5;

  static finalization::Params GetFinalizationParams() {
    finalization::Params params;
    params.epoch_length = epoch_length;
    return params;
  }

  Fixture()
    : repo(GetFinalizationParams()),
      commits(&active_chain, &repo, /*finalization::StateProcessor*/ nullptr) {

    active_chain.mock_AtHeight.SetStub([this](blockchain::Height h) -> CBlockIndex * {
      auto const it = this->m_block_heights.find(h);
      if (it == this->m_block_heights.end()) {
        return nullptr;
      }
      return it->second;
    });
    active_chain.mock_GetBlockIndex.SetStub([this](const uint256 &hash) -> CBlockIndex * {
      const auto it = m_block_indexes.find(hash);
      if (it == m_block_indexes.end()) {
        return nullptr;
      }
      return &it->second;
    });
  }

  CBlockIndex &CreateBlockIndex() {
    const auto height = FindNextHeight();
    const auto ins_res = m_block_indexes.emplace(uint256S(std::to_string(height)), CBlockIndex());
    CBlockIndex &index = ins_res.first->second;
    index.nHeight = height;
    index.phashBlock = &ins_res.first->first;
    index.pprev = m_block_heights[height - 1];
    active_chain.mock_GetTip.SetResult(&index);
    m_block_heights[index.nHeight] = &index;
    if (height == 0) {
      active_chain.mock_GetGenesis.SetResult(&index);
    }
    return index;
  }

  void AddBlocks(const size_t amount) {
    for (size_t i = 0; i < amount; ++i) {
      CreateBlockIndex();
    }
  }

  mocks::ActiveChainFake active_chain;
  mocks::StateRepositoryMock repo;
  FinalizerCommitsHandlerSpy commits;

 private:
  blockchain::Height FindNextHeight() {
    if (active_chain.GetTip() == nullptr) {
      return 0;
    } else {
      return active_chain.GetTip()->nHeight + 1;
    }
  }

  std::map<uint256, CBlockIndex> m_block_indexes;
  std::map<blockchain::Height, CBlockIndex *> m_block_heights; // m_block_index owns these block indexes
};

}

BOOST_FIXTURE_TEST_SUITE(finalizer_commits_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(get_commits_locator) {
  Fixture fixture;
  fixture.AddBlocks(1);  // add genesis

  staking::ActiveChain &chain = fixture.active_chain;
  FinalizationStateSpy &state = fixture.repo.state;
  p2p::FinalizerCommitsHandler &commits = fixture.commits;

  BOOST_REQUIRE(state.GetEpochLength() == 5);

  // Fill chain right before 1st checkpoint.
  fixture.AddBlocks(4);
  BOOST_REQUIRE(state.GetLastFinalizedEpoch() == 0);

  // Check `start` has Genesis as first finalized checkpoint.
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(*chain.AtHeight(1), chain.GetTip());
    const std::vector<uint256> expected_start = {
      chain.AtHeight(0)->GetBlockHash(),
      chain.AtHeight(1)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, chain.GetTip()->GetBlockHash());
  }

  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(*chain.AtHeight(2), nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(0)->GetBlockHash(),
      chain.AtHeight(2)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // Check that locator includes index that isn't present on the active chain
  // but has common block with it.
  {
    uint256 hash = uint256S("42");
    CBlockIndex index;
    index.phashBlock = &hash;
    index.nHeight = 1;
    index.pprev = const_cast<CBlockIndex*>(chain.AtHeight(0));
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(index, nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(0)->GetBlockHash(),
      hash,
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // Complete 1st epoch.
  fixture.AddBlocks(1);

  // Check 1st checkpoint is included in locator.
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(*chain.AtHeight(5), nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(0)->GetBlockHash(),
      chain.AtHeight(5)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // Check locator.start is limited by 3rd block.
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(*chain.AtHeight(3), nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(0)->GetBlockHash(),
      chain.AtHeight(3)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // Start 1st epoch.
  fixture.AddBlocks(1);

  // Check that 0th checkpoint is included in the locator.
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(*chain.AtHeight(6), nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(0)->GetBlockHash(),
      chain.AtHeight(5)->GetBlockHash(),
      chain.AtHeight(6)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // Check that start == checkpoint isn't included in locator twice.
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(*chain.AtHeight(5), nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(0)->GetBlockHash(),
      chain.AtHeight(5)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // Check locator.start is limited by 3rd block.
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(*chain.AtHeight(3), nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(0)->GetBlockHash(),
      chain.AtHeight(3)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // Generate blocks to complete 3 epochs and start 4th.
  // 0th epoch already completed.
  fixture.AddBlocks(4 + 5 + 2); // 1st epoch + 2nd epoch + two blocks of 3rd.

  // Make 1st epoch finalized
  state.SetLastFinalizedEpoch(2);
  BOOST_REQUIRE(state.GetLastFinalizedEpoch() == 2);

  // Check locator starts with last finalized checkpoint
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(*chain.AtHeight(12), nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(10)->GetBlockHash(),
      chain.AtHeight(12)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // Check locator includes checkpoint.
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(*chain.AtHeight(16), nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(10)->GetBlockHash(),
      chain.AtHeight(15)->GetBlockHash(),
      chain.AtHeight(16)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // When start == last_finalized_checkpoint, check locator includes only it.
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(*chain.AtHeight(10), nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(10)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // Check locator fallback to the active chain tip when start < last_finalized_checkpoint.
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(*chain.AtHeight(8), nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(10)->GetBlockHash(),
      chain.AtHeight(15)->GetBlockHash(),
      chain.AtHeight(17)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // Build a fork after finalization
  //           F
  // 0 .. 4 .. 9 .. 11 12 ..    -- main chain
  //                 |
  //                 > 12 .. 17 -- fork
  std::map<blockchain::Height, uint256> fork_hashes;
  std::map<blockchain::Height, CBlockIndex> fork;

  CBlockIndex *prev = const_cast<CBlockIndex*>(chain.AtHeight(11));
  for (blockchain::Height h = 12; h < 17; ++h) {
    fork_hashes[h] = uint256S(std::to_string(1000 + h));
    fork[h].phashBlock = &fork_hashes[h];
    fork[h].nHeight = h;
    fork[h].pprev = prev;
    prev = &fork[h];
  }

  // Check locator works on the fork
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(fork[16], nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(10)->GetBlockHash(),
      fork[15].GetBlockHash(),
      fork[16].GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }

  // Move finalization to checkpoint 15
  // Build a fork after finalization
  //                          F
  // 0 .. 4 .. 9 .. 11 12 .. 15 ..    -- main chain
  //                 |
  //                 > 12 .. 15 .. 17 -- fork
  state.SetLastFinalizedEpoch(3);
  BOOST_REQUIRE(state.GetLastFinalizedEpoch() == 3);

  // Check locator doesn't consider fork started before last_finalized_checkpoint
  {
    const p2p::FinalizerCommitsLocator locator =
      commits.GetFinalizerCommitsLocator(fork[16], nullptr);
    const std::vector<uint256> expected_start = {
      chain.AtHeight(15)->GetBlockHash(),
      chain.AtHeight(17)->GetBlockHash(),
    };
    BOOST_CHECK_EQUAL(locator.start, expected_start);
    BOOST_CHECK_EQUAL(locator.stop, uint256());
  }
}

const CBlockIndex *nullindex = nullptr;

BOOST_AUTO_TEST_CASE(find_most_recent_start) {
  Fixture fixture;
  fixture.AddBlocks(1);  // add genesis

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
    BOOST_CHECK_EQUAL(result, nullindex);
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

  state.SetLastFinalizedEpoch(2);
  fixture.AddBlocks(16);
  state.SetLastFinalizedEpoch(3); // block 15
  BOOST_REQUIRE(state.GetLastFinalizedEpoch() == 3);

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(13)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullindex);
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(10)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(10));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(10)->GetBlockHash(),
          chain.AtHeight(15)->GetBlockHash(),
          chain.AtHeight(20)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(20));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(10)->GetBlockHash(),
          chain.AtHeight(15)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(15));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(10)->GetBlockHash(),
          chain.AtHeight(20)->GetBlockHash(),
          chain.AtHeight(15)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(20));
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
          chain.AtHeight(15)->GetBlockHash(),
          fork[20].GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(15));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(15)->GetBlockHash(),
          fork[20].GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(15));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          fork[20].GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullindex);
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(10)->GetBlockHash(),
          fork[20].GetBlockHash(),
          chain.AtHeight(15)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(10));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(20)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullindex);
  }

  state.SetLastFinalizedEpoch(4); // block 20
  BOOST_REQUIRE(state.GetLastFinalizedEpoch() == 4);

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(20)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(20));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(18)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullindex);
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(21)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullindex);
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          chain.AtHeight(20)->GetBlockHash(),
          chain.AtHeight(21)->GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(21));
  }

  {
    const CBlockIndex *result = commits.FindMostRecentStart(Locator{{
          fork[20].GetBlockHash(),
        }, uint256()});
    BOOST_CHECK_EQUAL(result, nullindex);
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
    BOOST_CHECK_EQUAL(result, nullindex);
  }

  {
    const CBlockIndex *result = commits.FindStop(Locator{
        {}, uint256S("12345")});
    BOOST_CHECK_EQUAL(result, nullindex);
  }

  for (blockchain::Height h = 1; h < 5; ++h) {
    const CBlockIndex *result = commits.FindStop(Locator{
        {}, chain.AtHeight(h)->GetBlockHash()});
    BOOST_CHECK_EQUAL(result, chain.AtHeight(h));
  }
}

BOOST_AUTO_TEST_CASE(is_same_fork_test) {
  auto build_chain = [](const size_t size, CBlockIndex *prev = nullptr) {
    std::map<blockchain::Height, CBlockIndex> indexes;
    blockchain::Height h = prev != nullptr ? prev->nHeight + 1 : 0;
    for (size_t i = 0; i < size; ++i) {
      indexes[h + i].pprev = prev;
      indexes[h + i].nHeight = h + i;
      prev = &indexes[h + i];
    }
    return indexes;
  };

  auto is_same_fork = FinalizerCommitsHandlerSpy::IsSameFork;

  // Check in random order
  {
    auto chain = build_chain(10);
    const CBlockIndex *prev = nullptr;
    BOOST_CHECK_EQUAL(is_same_fork(&chain[9], &chain[9], prev), true);
    BOOST_CHECK_EQUAL(is_same_fork(&chain[9], &chain[2], prev), true);
    BOOST_CHECK_EQUAL(is_same_fork(&chain[9], &chain[5], prev), true);
    BOOST_CHECK_EQUAL(is_same_fork(&chain[9], &chain[1], prev), true);
    BOOST_CHECK_EQUAL(is_same_fork(&chain[2], &chain[3], prev), false);
  }

  // Check when heights sorted, prev optimization should work.
  {
    auto chain = build_chain(10);
    const CBlockIndex *prev = nullptr;
    BOOST_CHECK_EQUAL(is_same_fork(&chain[9], &chain[0], prev), true);
    BOOST_CHECK_EQUAL(prev, &chain[0]);
    BOOST_CHECK_EQUAL(is_same_fork(&chain[9], &chain[1], prev), true);
    BOOST_CHECK_EQUAL(prev, &chain[1]);
    BOOST_CHECK_EQUAL(is_same_fork(&chain[9], &chain[2], prev), true);
    BOOST_CHECK_EQUAL(prev, &chain[2]);
    BOOST_CHECK_EQUAL(is_same_fork(&chain[9], &chain[3], prev), true);
    BOOST_CHECK_EQUAL(prev, &chain[3]);
  }

  {
    auto chain = build_chain(10);
    auto fork = build_chain(10, &chain[3]);
    BOOST_REQUIRE(fork[4].pprev == &chain[3]);
    const CBlockIndex *prev = nullptr;
    BOOST_CHECK_EQUAL(is_same_fork(&chain[4], &fork[4], prev), false);
    BOOST_CHECK_EQUAL(is_same_fork(&fork[4], &chain[3], prev), true);
    BOOST_CHECK_EQUAL(is_same_fork(&fork[4], &chain[3], prev), true);
    BOOST_CHECK_EQUAL(is_same_fork(&fork[10], &fork[4], prev), true);
    BOOST_CHECK_EQUAL(is_same_fork(&fork[9], &chain[9], prev), false);
  }
}

BOOST_AUTO_TEST_SUITE_END()
