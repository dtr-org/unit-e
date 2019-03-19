// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/finalization_reward_logic.h>

#include <blockdb.h>
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
  bool RestoreFromDisk(Dependency<finalization::StateProcessor> proc) override { return false; }
  bool SaveToDisk() override { return false; }
  bool Restoring() const override { return false; }
  void Reset(const esperanza::FinalizationParams &params, const esperanza::AdminParams &admin_params) override {}
  void ResetToTip(const CBlockIndex &block_index) override {}
  void TrimUntilHeight(blockchain::Height height) override {}
  const esperanza::FinalizationParams &GetFinalizationParams() const override { return fin_params; }
  const esperanza::AdminParams &GetAdminParams() const override { return admin_params; }
};

class FakeBlockDB : public ::BlockDB {
 public:
  std::vector<CBlock> blocks;

  boost::optional<CBlock> ReadBlock(const CBlockIndex &index) override {
    if (index.nHeight < blocks.size()) {
      return blocks[index.nHeight];
    }
    return boost::none;
  }
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
  FakeBlockDB block_db;
  std::vector<CBlock> blocks;
  std::vector<CBlockIndex> block_indices;

  void AddState(blockchain::Height height, const finalization::FinalizationState &state) {
    state_repository.states.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(&BlockIndexAtHeight(height)),
        std::forward_as_tuple(state, finalization::FinalizationState::COMPLETED));
  }

  CTransactionRef MakeCoinbaseTx(blockchain::Height height, const WitnessV0KeyHash &dest) {
    CMutableTransaction tx;
    auto value = behavior->CalculateBlockReward(height);
    auto script = GetScriptForDestination(dest);
    tx.vout.emplace_back(value, script);
    return MakeTransactionRef(tx);
  }

  const CBlockIndex &BlockIndexAtHeight(blockchain::Height h) {
    return block_indices.at(h);
  }

  const CBlock &BlockAtHeight(blockchain::Height h) {
    return blocks.at(h);
  }

  void BuildChain(blockchain::Height max_height) {
    blocks.resize(max_height);
    block_indices.resize(max_height);
    for (blockchain::Height h = 1; h <= max_height; ++h) {
      blocks[h].hashPrevBlock = blocks[h - 1].GetHash();
      std::vector<unsigned char> dest(20, static_cast<unsigned char>(h));
      blocks[h].vtx.push_back(MakeCoinbaseTx(h, WitnessV0KeyHash(dest)));
      blocks[h].ComputeMerkleTrees();

      block_indices[h].nHeight = static_cast<int>(h);
      block_indices[h].pprev = &block_indices[h - 1];
    }
    block_db.blocks = blocks;
  }

  std::unique_ptr<proposer::FinalizationRewardLogic> GetFinalizationRewardLogic() {
    return proposer::FinalizationRewardLogic::New(behavior.get(), &state_repository, &block_db);
  }
};

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

  auto first_epoch_checkpoint = f.fin_params.GetEpochCheckpointHeight(1);
  f.BuildChain(first_epoch_checkpoint + f.fin_params.epoch_length + 1);

  auto test_height = first_epoch_checkpoint;

  f.AddState(test_height, fin_state);
  // We must pay out the rewards at the first block of an epoch, i.e. when the current tip is a checkpoint block
  std::vector<std::pair<CScript, CAmount>> rewards = logic->GetFinalizationRewards(f.BlockIndexAtHeight(test_height));
  BOOST_CHECK_EQUAL(rewards.size(), f.fin_params.epoch_length);
  for (std::size_t i = 0; i < rewards.size(); ++i) {
    auto h = static_cast<blockchain::Height>(fin_state.GetEpochStartHeight(1) + i);
    auto r = static_cast<CAmount>(f.parameters.reward_function(f.parameters, h) * 0.4);
    BOOST_CHECK_EQUAL(rewards[i].second, r);
    auto s = f.BlockAtHeight(h).vtx[0]->vout[0].scriptPubKey;
    BOOST_CHECK_EQUAL(HexStr(rewards[i].first), HexStr(s));
  }

  fin_state.InitializeEpoch(fin_state.GetEpochStartHeight(2));
  BOOST_CHECK_EQUAL(fin_state.GetLastFinalizedEpoch(), 0);
  BOOST_CHECK_EQUAL(fin_state.GetCurrentEpoch(), 2);

  ++test_height;
  for (; test_height < f.fin_params.GetEpochCheckpointHeight(2); ++test_height) {
    f.AddState(test_height, fin_state);
    rewards = logic->GetFinalizationRewards(f.BlockIndexAtHeight(test_height));
    BOOST_CHECK_EQUAL(rewards.size(), 0);
  }

  // Calculate rewards at the checkpoint of the second epoch
  f.AddState(test_height, fin_state);
  rewards = logic->GetFinalizationRewards(f.BlockIndexAtHeight(test_height));
  BOOST_CHECK_EQUAL(rewards.size(), f.fin_params.epoch_length);
  for (std::size_t i = 0; i < rewards.size(); ++i) {
    auto h = static_cast<blockchain::Height>(fin_state.GetEpochStartHeight(2) + i);
    auto r = static_cast<CAmount>(f.parameters.reward_function(f.parameters, h) * 0.4);
    BOOST_CHECK_EQUAL(rewards[i].second, r);
    auto s = f.BlockAtHeight(h).vtx[0]->vout[0].scriptPubKey;
    BOOST_CHECK_EQUAL(HexStr(rewards[i].first), HexStr(s));
  }
}

BOOST_AUTO_TEST_SUITE_END()
