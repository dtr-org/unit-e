// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_behavior.h>

namespace blockchain {

Behavior::Behavior(const Parameters &parameters) : m_parameters(parameters) {}

Difficulty Behavior::CalculateDifficulty(Height height, ChainAccess &chain) const {
  return m_parameters.difficulty_function(m_parameters, height, chain);
};

std::uint32_t Behavior::CalculateProposingTimestamp(std::uint64_t timestamp_sec) const {
  auto blocktime = static_cast<std::uint32_t>(timestamp_sec);
  blocktime = blocktime - (blocktime % m_parameters.block_stake_timestamp_interval_seconds);
  return blocktime;
}

std::uint32_t Behavior::CalculateProposingTimestampAfter(std::uint64_t time) const {
  return CalculateProposingTimestamp(time) + m_parameters.block_stake_timestamp_interval_seconds;
}

CAmount Behavior::CalculateReward(const MoneySupply supply, const Height height) const {
  return m_parameters.reward_function(m_parameters, supply, height);
}

const CBlock &Behavior::GetGenesisBlock() const {
  return m_parameters.genesis_block->block;
}

uint256 Behavior::GetGenesisBlockHash() const {
  return m_parameters.genesis_block->hash;
}

std::string Behavior::GetNetworkName() const {
  return std::string(m_parameters.network_name);
}

std::unique_ptr<Behavior> Behavior::New(Dependency<::ArgsManager> args) {
  if (args->GetBoolArg("-regtest", false)) {
    return MakeUnique<blockchain::Behavior>(Parameters::RegTest());
  } else if (args->GetBoolArg("-testnet", false)) {
    return MakeUnique<blockchain::Behavior>(Parameters::TestNet());
  } else {
    return MakeUnique<blockchain::Behavior>(Parameters::MainNet());
  }
}

std::unique_ptr<Behavior> Behavior::FromParameters(const Parameters& parameters) {
  return MakeUnique<blockchain::Behavior>(parameters);
}

}  // namespace blockchain
