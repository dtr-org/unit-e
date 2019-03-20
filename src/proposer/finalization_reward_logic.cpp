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

  std::vector<std::pair<CScript, CAmount>> GetFinalizationRewards(const CBlockIndex *last_blk) override {
    return {};
  }
};

std::unique_ptr<FinalizationRewardLogic> FinalizationRewardLogic::New(
    Dependency<blockchain::Behavior> behaviour,
    Dependency<finalization::StateRepository> repo) {
  return MakeUnique<FinalizationRewardLogicImpl>(behaviour, repo);
}

}  // namespace proposer
