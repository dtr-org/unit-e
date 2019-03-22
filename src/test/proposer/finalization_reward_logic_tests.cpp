// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/finalization_reward_logic.h>

#include <finalization/state_repository.h>
#include <staking/validation_result.h>

#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
#include <boost/test/unit_test.hpp>

#include <functional>

namespace {

class FakeStateRepository : public finalization::StateRepository {
 public:
  esperanza::FinalizationParams fin_params;
  esperanza::AdminParams admin_params;
  std::map<const CBlockIndex *, finalization::FinalizationState> states;

  finalization::FinalizationState *GetTipState() override {
    return nullptr;
  }
  finalization::FinalizationState *Find(const CBlockIndex &block_index) override {
    const auto it = states.find(&block_index);
    if (it == states.end()) {
      return nullptr;
    }
    return &it->second;
  }
  finalization::FinalizationState *FindOrCreate(const CBlockIndex &,
                                                esperanza::FinalizationState::InitStatus) override {
    return nullptr;
  }
  bool Confirm(const CBlockIndex &block_index,
               finalization::FinalizationState &&new_state,
               finalization::FinalizationState **state_out) override {
    return false;
  }
  void RestoreFromDisk(const CChainParams &chainparams, Dependency<finalization::StateProcessor> proc) override {}
  bool Restoring() const override { return false; }
  void Reset(const esperanza::FinalizationParams &params, const esperanza::AdminParams &admin_params) override {}
  void ResetToTip(const CBlockIndex &block_index) override {}
  void TrimUntilHeight(blockchain::Height height) override {}
  const esperanza::FinalizationParams &GetFinalizationParams() const override { return fin_params; }
  const esperanza::AdminParams &GetAdminParams() const override { return admin_params; }
};

struct Fixture {
  esperanza::FinalizationParams fin_params;
  esperanza::AdminParams admin_params;
  blockchain::Parameters parameters = [this]() {
    auto p = blockchain::Parameters::TestNet();
    p.reward_schedule = {10000, 9000, 8000, 7000, 6000, 5000, 4000, 3000, 2000, 1000};
    p.period_blocks = fin_params.epoch_length - 1; // To have different rewards within each epoch
    return p;
  }();
  std::unique_ptr<blockchain::Behavior> behavior = blockchain::Behavior::NewFromParameters(parameters);

  FakeStateRepository state_repository;
  void AddState(const CBlockIndex *index, const finalization::FinalizationState &state) {
    state_repository.states.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(index),
        std::forward_as_tuple(state, finalization::FinalizationState::COMPLETED));
  }

  std::unique_ptr<proposer::FinalizationRewardLogic> GetFinalizationRewardLogic() {
    return proposer::FinalizationRewardLogic::New(behavior.get(), &state_repository);
  }
};

std::vector<CBlockIndex> GenerateBlockIndices(blockchain::Height start_height, std::size_t block_count) {
  std::vector<CBlockIndex> indices(block_count);
  indices[0].nHeight = static_cast<int>(start_height);
  for (std::size_t i = 1; i < indices.size(); ++i) {
    indices[i].pprev = &indices[i - 1];
    indices[i].nHeight = indices[i - 1].nHeight + 1;
  }
  return indices;
}

}  // namespace

BOOST_AUTO_TEST_SUITE(finalization_reward_logic_tests)

BOOST_AUTO_TEST_CASE(get_finalization_rewards) {
  Fixture f;
  auto logic = f.GetFinalizationRewardLogic();
  auto fin_state = finalization::FinalizationState(f.fin_params, f.admin_params);

  // TODO UNIT-E: test that we don't pay rewards at the block 1 once https://github.com/dtr-org/unit-e/pull/809 is merged

  fin_state.InitializeEpoch(fin_state.GetEpochStartHeight(1));
  BOOST_CHECK_EQUAL(fin_state.GetLastFinalizedEpoch(), 0);
  BOOST_CHECK_EQUAL(fin_state.GetCurrentEpoch(), 1);

  auto start_height = fin_state.GetEpochCheckpointHeight(1);
  std::vector<CBlockIndex> blocks = GenerateBlockIndices(start_height, fin_state.GetEpochLength() + 1);

  f.AddState(&blocks[0], fin_state);

  // We must pay out the rewards at the first block of an epoch, i.e. when the current tip is a checkpoint block
  std::vector<std::pair<CScript, CAmount>> rewards = logic->GetFinalizationRewards(blocks[0]);
  BOOST_CHECK_EQUAL(rewards.size(), fin_state.GetEpochLength());
  for (std::size_t i = 0; i < rewards.size(); ++i) {
    auto h = static_cast<blockchain::Height>(fin_state.GetEpochStartHeight(1) + i);
    auto r = static_cast<CAmount>(f.parameters.reward_function(f.parameters, h) * 0.4);
    BOOST_CHECK_EQUAL(rewards[i].second, r);
  }

  fin_state.InitializeEpoch(fin_state.GetEpochStartHeight(2));
  BOOST_CHECK_EQUAL(fin_state.GetLastFinalizedEpoch(), 0);
  BOOST_CHECK_EQUAL(fin_state.GetCurrentEpoch(), 2);

  for (std::size_t i = 1; i < blocks.size() - 1; ++i) {
    f.AddState(&blocks[i], fin_state);
    rewards = logic->GetFinalizationRewards(blocks[i]);
    BOOST_CHECK_EQUAL(rewards.size(), 0);
  }

  f.AddState(&blocks.back(), fin_state);
  rewards = logic->GetFinalizationRewards(blocks.back());
  BOOST_CHECK_EQUAL(rewards.size(), fin_state.GetEpochLength());
  for (std::size_t i = 0; i < rewards.size(); ++i) {
    auto h = static_cast<blockchain::Height>(fin_state.GetEpochStartHeight(2) + i);
    auto r = static_cast<CAmount>(f.parameters.reward_function(f.parameters, h) * 0.4);
    BOOST_CHECK_EQUAL(rewards[i].second, r);
  }
}

BOOST_AUTO_TEST_SUITE_END()
