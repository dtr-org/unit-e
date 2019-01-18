// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer_logic.h>

namespace proposer {

class LogicImpl final : public Logic {

 private:
  Dependency<blockchain::Behavior> m_blockchain_behavior;
  Dependency<staking::Network> m_network;
  Dependency<staking::ActiveChain> m_active_chain;
  Dependency<staking::StakeValidator> m_stake_validator;

 public:
  LogicImpl(
      Dependency<blockchain::Behavior> blockchain_behavior,
      Dependency<staking::Network> network,
      Dependency<staking::ActiveChain> active_chain,
      Dependency<staking::StakeValidator> stake_validator)
      : m_blockchain_behavior(blockchain_behavior),
        m_network(network),
        m_active_chain(active_chain),
        m_stake_validator(stake_validator) {}

  // An implementation of the Proof-of-Stake proposing/mining algorithm.
  //
  // foreach (utxo in wallet) {
  //   kernelhash = hash(prevBlock.stakeModifier ++ utxo.time ++ utxo.hash ++ utxo.n ++ blockTime)
  //   if (kernelhash < difficulty * utxo.value) {
  //     block.stakeModifier = hash(kernelhash ++ prevBlock.stakeModifier)
  //     propose(block);
  //     return;
  //   }
  // }
  //
  // The details as for how to calculate the kernel hash and check a valid kernel
  // are left up to the injectable staking::StakeValidator.
  //
  // The part of actually proposing (`propose(block)`) is left up to the caller
  // of this function (the `Proposer`, see proposer.cpp).
  boost::optional<proposer::EligibleCoin> TryPropose(const std::vector<staking::Coin> &eligible_utxos) override {
    AssertLockHeld(m_active_chain->GetLock());

    const CBlockIndex *current_tip =
        m_active_chain->GetTip();
    if (!current_tip) {
      return boost::none;
    }
    const blockchain::Height current_height =
        m_active_chain->GetHeight();
    const blockchain::Height target_height =
        current_height + 1;
    const blockchain::Time target_time =
        m_blockchain_behavior->CalculateProposingTimestampAfter(m_network->GetTime());
    const blockchain::Difficulty target_difficulty =
        m_blockchain_behavior->CalculateDifficulty(target_height, *m_active_chain);

    for (const auto &coin : eligible_utxos) {
      const uint256 kernel_hash = m_stake_validator->ComputeKernelHash(current_tip, coin, target_time);
      if (m_stake_validator->CheckKernel(coin.amount, kernel_hash, target_difficulty)) {
        const CAmount reward = m_blockchain_behavior->CalculateBlockReward(
            target_height);
        return {{coin,
                 kernel_hash,
                 reward,
                 target_height,
                 target_time,
                 target_difficulty}};
      }
    }
    return boost::none;
  }
};

std::unique_ptr<Logic> Logic::New(
    Dependency<blockchain::Behavior> blockchain_behavior,
    Dependency<staking::Network> network,
    Dependency<staking::ActiveChain> active_chain,
    Dependency<staking::StakeValidator> stake_validator) {
  return std::unique_ptr<Logic>(new LogicImpl(blockchain_behavior, network, active_chain, stake_validator));
}

}  // namespace proposer
