// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_behavior.h>

namespace blockchain {

Behavior::Behavior(const Parameters &parameters) : m_parameters(parameters) {}

Difficulty Behavior::CalculateDifficulty(BlockHeight blockHeight, ChainAccess &chainAccess) const {
  return m_parameters.difficultyFunction(m_parameters, blockHeight, chainAccess);
};

std::uint32_t Behavior::CalculateProposingTimestamp(std::uint64_t unixtimeSeconds) const {
  auto blocktime = static_cast<std::uint32_t>(unixtimeSeconds);
  blocktime = blocktime - (blocktime % m_parameters.blockStakeTimestampIntervalSeconds);
  return blocktime;
}

std::uint32_t Behavior::CalculateProposingTimestampAfter(std::uint64_t unixtimeSeconds) const {
  return CalculateProposingTimestamp(unixtimeSeconds) + m_parameters.blockStakeTimestampIntervalSeconds;
}

CAmount Behavior::CalculateReward(const MoneySupply moneySupply, const BlockHeight blockHeight) const {
  return m_parameters.rewardFunction(m_parameters, moneySupply, blockHeight);
}

const CBlock &Behavior::GetGenesisBlock() const {
  return m_parameters.genesisBlock->block;
}

uint256 Behavior::GetGenesisBlockHash() const {
  return m_parameters.genesisBlock->hash;
}

std::string Behavior::GetNetworkName() const {
  return std::string(m_parameters.networkName);
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

}  // namespace blockchain
