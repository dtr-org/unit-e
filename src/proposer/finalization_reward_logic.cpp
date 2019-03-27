// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/finalization_reward_logic.h>
#include <algorithm>

namespace proposer {

class FinalizationRewardLogicImpl : public FinalizationRewardLogic {
 private:
  Dependency<blockchain::Behavior> m_blockchain_behavior;
  Dependency<finalization::StateRepository> m_fin_state_repo;
  Dependency<BlockDB> m_block_db;

 public:
  FinalizationRewardLogicImpl(Dependency<blockchain::Behavior> blockchain_behavior,
                              Dependency<finalization::StateRepository> repo,
                              Dependency<BlockDB> block_db)
      : m_blockchain_behavior(blockchain_behavior),
        m_fin_state_repo(repo),
        m_block_db(block_db) {
  }

  std::vector<std::pair<CScript, CAmount>> GetFinalizationRewards(const CBlockIndex &last_block) override {
    const auto fin_state = m_fin_state_repo->Find(last_block);
    assert(fin_state);

    auto prev_height = static_cast<blockchain::Height>(last_block.nHeight);
    if (!fin_state->IsCheckpoint(prev_height)) {
      return {};
    }

    std::vector<std::pair<CScript, CAmount>> result;
    result.reserve(fin_state->GetEpochLength());
    auto pblock = &last_block;
    blockchain::Height epoch_start = fin_state->GetEpochStartHeight(fin_state->GetCurrentEpoch());
    for (auto h = prev_height; h >= epoch_start; --h) {
      assert(pblock && pblock->nHeight == h);
      auto block = m_block_db->ReadBlock(*pblock);
      if (!block) {
        // TODO UNIT-E
      }
      const auto &reward_script = block->vtx[0]->vout[0].scriptPubKey;
      result.emplace_back(reward_script, m_blockchain_behavior->CalculateFinalizationReward(h));
      pblock = pblock->pprev;
    }
    assert(result.size() == fin_state->GetEpochLength());
    std::reverse(result.begin(), result.end());
    return result;
  }
};

std::unique_ptr<FinalizationRewardLogic> FinalizationRewardLogic::New(
    Dependency<blockchain::Behavior> behaviour,
    Dependency<finalization::StateRepository> repo,
    Dependency<BlockDB> block_db) {
  return MakeUnique<FinalizationRewardLogicImpl>(behaviour, repo, block_db);
}

}  // namespace proposer
