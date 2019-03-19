// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_PROPOSER_FINALIZATION_REWARD_LOGIC_H
#define UNITE_PROPOSER_FINALIZATION_REWARD_LOGIC_H

#include <blockchain/blockchain_behavior.h>
#include <blockdb.h>
#include <dependency.h>
#include <finalization/state_repository.h>

#include <vector>

class CWallet;

namespace proposer {

class FinalizationRewardLogic {

 public:
  virtual std::vector<std::pair<CScript, CAmount>> GetFinalizationRewards(const CBlockIndex &) = 0;

  virtual ~FinalizationRewardLogic() = default;

  static std::unique_ptr<FinalizationRewardLogic> New(
      Dependency<blockchain::Behavior>,
      Dependency<finalization::StateRepository>,
      Dependency<BlockDB>);
};

}  // namespace proposer

#endif  // UNITE_PROPOSER_FINALIZATION_REWARD_LOGIC_H
