// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/finalization_reward_logic.h>

namespace proposer {

class FinalizationRewardLogicImpl : public FinalizationRewardLogic {
 private:
  Dependency<blockchain::Behavior> m_blockchain_behavior;
  Dependency<finalization::StateRepository> m_fin_state_repo;

 public:
  FinalizationRewardLogicImpl(Dependency<blockchain::Behavior> blockchain_behavior,
                              Dependency<finalization::StateRepository> repo)
      : m_blockchain_behavior(blockchain_behavior),
        m_fin_state_repo(repo) {
  }

  std::vector<std::pair<CScript, CAmount>> GetFinalizationRewards(const CBlockIndex &last_blk) override {
    const auto fin_state = m_fin_state_repo->Find(last_blk);
    assert(fin_state);
    if (!fin_state->IsCheckpoint(static_cast<blockchain::Height>(last_blk.nHeight))) {
      return {};
    }
    // TODO UNIT-E: check if we can get prev_checkpoint_blk and prev_checkpoint_state after ISD
    auto prev_checkpoint_blk = &last_blk;
    for (uint32_t i = 0; i < fin_state->GetEpochLength(); ++i) {
      prev_checkpoint_blk = prev_checkpoint_blk->pprev;
    }
    const auto prev_checkpoint_state = m_fin_state_repo->Find(*prev_checkpoint_blk);
    assert(prev_checkpoint_blk);

    uint32_t cur_finalized_epoch = fin_state->GetLastFinalizedEpoch();
    uint32_t prev_finalized_epoch = prev_checkpoint_state->GetLastFinalizedEpoch();
    if (cur_finalized_epoch == prev_finalized_epoch) {
      // Finalization did not happen
      return {};
    }
    std::vector<std::pair<CScript, CAmount>> result;
    for (auto e = prev_finalized_epoch; e <= cur_finalized_epoch; ++e) {
      blockchain::Height next_epoch_start = fin_state->GetEpochStartHeight(e + 1);
      for (auto h = fin_state->GetEpochStartHeight(e); h < next_epoch_start; ++h) {
        result.emplace_back(CScript(), 0);
      }
    }
    return result;
  }
};

std::unique_ptr<FinalizationRewardLogic> FinalizationRewardLogic::New(
    Dependency<blockchain::Behavior> behaviour,
    Dependency<finalization::StateRepository> repo) {
  return MakeUnique<FinalizationRewardLogicImpl>(behaviour, repo);
}

}  // namespace proposer
