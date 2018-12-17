// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_behavior.h>

namespace blockchain {

Behavior::Behavior(const Parameters &parameters) noexcept
    : m_parameters(parameters) {}

Difficulty Behavior::CalculateDifficulty(Height height, ChainAccess &chain) const {
  return m_parameters.difficulty_function(m_parameters, height, chain);
};

Time Behavior::CalculateProposingTimestamp(std::int64_t timestamp_sec) const {
  auto blocktime = static_cast<std::uint32_t>(timestamp_sec);
  blocktime = blocktime - (blocktime % m_parameters.block_stake_timestamp_interval_seconds);
  return blocktime;
}

Time Behavior::CalculateProposingTimestampAfter(std::int64_t time) const {
  return CalculateProposingTimestamp(time) + m_parameters.block_stake_timestamp_interval_seconds;
}

CAmount Behavior::CalculateReward(const MoneySupply supply, const Height height) const {
  return m_parameters.reward_function(m_parameters, supply, height);
}

uint256 Behavior::GetGenesisBlockHash() const {
  return m_parameters.genesis_block->hash;
}

const CBlock &Behavior::GetGenesisBlock() const {
  return m_parameters.genesis_block->block;
}

bool Behavior::IsGenesisBlockHash(const uint256 &hash) const {
  return hash == GetGenesisBlockHash();
}

bool Behavior::IsGenesisBlock(const CBlock &block) const {
  return IsGenesisBlockHash(block.GetHash());
}

const Parameters& Behavior::GetParameters() const {
  return m_parameters;
}

boost::optional<CPubKey> Behavior::ExtractBlockSigningKey(const CBlock &block) const {
  if (block.vtx.empty()) {
    return boost::none;
  }
  const auto &coinbase_inputs = block.vtx[0]->vin;
  if (coinbase_inputs.size() < 2) {
    return boost::none;
  }
  const auto &witnessStack = coinbase_inputs[1].scriptWitness.stack;
  if (witnessStack.size() < 2) {
    return boost::none;
  }
  // As per https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki#p2wpkh,
  // a P2WPKH transaction looks like this:
  //
  //    witness:      <signature> <pubkey>
  //    scriptSig:    (empty)
  //    scriptPubKey: 0 <20-byte-key-hash>
  //                  (0x0014{20-byte-key-hash})
  //
  // That is: The pubkey we're interested in is in stack[1]
  // (stack[0] is the signature).
  const auto &pubKeyData = witnessStack[1];
  CPubKey pubKey(pubKeyData.begin(), pubKeyData.end());
  if (!pubKey.IsValid()) {
    return boost::none;
  }
  return pubKey;
}

std::string Behavior::GetNetworkName() const {
  return std::string(m_parameters.network_name);
}

std::chrono::seconds Behavior::GetBlockStakeTimestampInterval() const {
  return std::chrono::seconds(m_parameters.block_stake_timestamp_interval_seconds);
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

std::unique_ptr<Behavior> Behavior::NewFromParameters(const Parameters &parameters) {
  return MakeUnique<blockchain::Behavior>(parameters);
}

}  // namespace blockchain
