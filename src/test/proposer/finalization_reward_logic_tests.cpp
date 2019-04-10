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

class BlockDBMock : public ::BlockDB {
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
    p.period_blocks = fin_params.epoch_length - 1;  // To have different rewards within each epoch
    return p;
  }();
  std::unique_ptr<blockchain::Behavior> behavior = blockchain::Behavior::NewFromParameters(parameters);

  mocks::StateRepositoryMock state_repository{fin_params};
  BlockDBMock block_db;
  std::vector<CBlock> blocks;
  std::vector<CBlockIndex> block_indices;

  CTransactionRef MakeCoinbaseTx(blockchain::Height height, const WitnessV0KeyHash &dest) {
    CMutableTransaction tx;
    auto value = behavior->CalculateBlockReward(height);
    auto script = GetScriptForDestination(dest);
    tx.vout.emplace_back(value, script);
    return MakeTransactionRef(tx);
  }

  const CBlockIndex &BlockIndexAtHeight(blockchain::Height h) { return block_indices.at(h); }

  const CBlock &BlockAtHeight(blockchain::Height h) { return blocks.at(h); }

  void BuildChain(blockchain::Height max_height) {
    blocks.resize(max_height + 1);
    block_indices.resize(max_height + 1);
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
  FinalizationStateSpy &fin_state = f.state_repository.state;

  f.BuildChain(f.fin_params.GetEpochCheckpointHeight(2) + 1);

  std::vector<std::pair<CScript, CAmount>> rewards = logic->GetFinalizationRewards(f.BlockIndexAtHeight(0));
  std::vector<CAmount> reward_amounts = logic->GetFinalizationRewardAmounts(f.BlockIndexAtHeight(0));
  BOOST_CHECK_EQUAL(rewards.size(), 0);
  BOOST_CHECK_EQUAL(reward_amounts.size(), 0);

  for (uint32_t epoch = 1; epoch < 3; ++epoch) {
    fin_state.InitializeEpoch(fin_state.GetEpochStartHeight(epoch));
    BOOST_REQUIRE_EQUAL(fin_state.GetCurrentEpoch(), epoch);

    for (auto height = fin_state.GetEpochStartHeight(epoch); height < f.fin_params.GetEpochCheckpointHeight(epoch);
         ++height) {
      rewards = logic->GetFinalizationRewards(f.BlockIndexAtHeight(height));
      reward_amounts = logic->GetFinalizationRewardAmounts(f.BlockIndexAtHeight(height));
      BOOST_CHECK_EQUAL(rewards.size(), 0);
      BOOST_CHECK_EQUAL(reward_amounts.size(), 0);
    }

    auto checkpoint_height = fin_state.GetEpochCheckpointHeight(epoch);

    // We must pay out the rewards in the first block of an epoch, i.e. when the current tip is a checkpoint block
    rewards = logic->GetFinalizationRewards(f.BlockIndexAtHeight(checkpoint_height));
    reward_amounts = logic->GetFinalizationRewardAmounts(f.BlockIndexAtHeight(checkpoint_height));
    BOOST_CHECK_EQUAL(rewards.size(), f.fin_params.epoch_length);
    BOOST_CHECK_EQUAL(reward_amounts.size(), f.fin_params.epoch_length);
    for (std::size_t i = 0; i < rewards.size(); ++i) {
      auto h = static_cast<blockchain::Height>(fin_state.GetEpochStartHeight(epoch) + i);
      auto r = static_cast<CAmount>(f.parameters.reward_function(f.parameters, h) * 0.4);
      BOOST_CHECK_EQUAL(rewards[i].second, r);
      BOOST_CHECK_EQUAL(reward_amounts[i], r);
      auto s = f.BlockAtHeight(h).vtx[0]->vout[0].scriptPubKey;
      BOOST_CHECK_EQUAL(HexStr(rewards[i].first), HexStr(s));
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
