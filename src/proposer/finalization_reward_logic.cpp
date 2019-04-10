// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/finalization_reward_logic.h>
#include <util.h>

#include <boost/optional.hpp>

#include <algorithm>

namespace proposer {

class FinalizationRewardLogicImpl : public FinalizationRewardLogic {
 private:
  const Dependency<blockchain::Behavior> m_blockchain_behavior;
  const Dependency<finalization::StateRepository> m_fin_state_repo;
  const Dependency<BlockDB> m_block_db;

  CScript GetRewardScript(const CBlockIndex &index) const {
    // TODO UNIT-E: implement more efficient reward script retrieval
    boost::optional<CBlock> block = m_block_db->ReadBlock(index);
    if (!block) {
      LogPrintf("Cannot read block=%s.\n", index.GetBlockHash().GetHex());
      throw MissingBlockError(index);
    }
    return block->vtx[0]->vout[0].scriptPubKey;
  }

 public:
  FinalizationRewardLogicImpl(const Dependency<blockchain::Behavior> blockchain_behavior,
                              const Dependency<finalization::StateRepository> repo,
                              const Dependency<BlockDB> block_db)
      : m_blockchain_behavior(blockchain_behavior),
        m_fin_state_repo(repo),
        m_block_db(block_db) {
  }

  std::vector<std::pair<CScript, CAmount>> GetFinalizationRewards(const CBlockIndex &last_block) const override {
    if (last_block.nHeight < m_fin_state_repo->GetFinalizationParams().GetEpochCheckpointHeight(1)) {
      return {};
    }

    auto prev_height = static_cast<blockchain::Height>(last_block.nHeight);
    if (!m_fin_state_repo->GetFinalizationParams().IsCheckpoint(prev_height)) {
      return {};
    }

    std::vector<std::pair<CScript, CAmount>> result;
    result.reserve(m_fin_state_repo->GetFinalizationParams().epoch_length);
    const CBlockIndex *pblock = &last_block;
    const auto epoch = m_fin_state_repo->GetFinalizationParams().GetEpoch(prev_height);
    const blockchain::Height epoch_start = m_fin_state_repo->GetFinalizationParams().GetEpochStartHeight(epoch);

    for (auto h = prev_height; h >= epoch_start; --h) {
      assert(pblock && pblock->nHeight == h);
      result.emplace_back(GetRewardScript(*pblock), m_blockchain_behavior->CalculateFinalizationReward(h));
      pblock = pblock->pprev;
    }
    assert(result.size() == m_fin_state_repo->GetFinalizationParams().epoch_length);
    std::reverse(result.begin(), result.end());
    return result;
  }

  std::size_t GetNumberOfRewardOutputs(const blockchain::Height current_height) const override {
    if (m_fin_state_repo->GetFinalizationParams().IsEpochStart(current_height) &&
        m_fin_state_repo->GetFinalizationParams().GetEpoch(current_height) > 1) {
      return m_fin_state_repo->GetFinalizationParams().epoch_length;
    }
    return 0;
  }
};

std::unique_ptr<FinalizationRewardLogic> FinalizationRewardLogic::New(
    const Dependency<blockchain::Behavior> behaviour,
    const Dependency<finalization::StateRepository> repo,
    const Dependency<BlockDB> block_db) {
  return MakeUnique<FinalizationRewardLogicImpl>(behaviour, repo, block_db);
}

}  // namespace proposer
