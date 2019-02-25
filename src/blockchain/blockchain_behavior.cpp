// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_behavior.h>

#include <blockchain/blockchain_custom_parameters.h>

namespace blockchain {

Behavior::Behavior(const Parameters &parameters) noexcept
    : m_parameters(parameters) {}

Difficulty Behavior::CalculateDifficulty(Height height, ChainAccess &chain) const {
  return m_parameters.difficulty_function(m_parameters, height, chain);
}

Time Behavior::CalculateProposingTimestamp(const std::int64_t timestamp_sec) const {
  auto blocktime = static_cast<std::uint32_t>(timestamp_sec);
  if (m_parameters.block_stake_timestamp_interval_seconds == 0) {
    return blocktime;
  }
  blocktime = blocktime - (blocktime % m_parameters.block_stake_timestamp_interval_seconds);
  return blocktime;
}

Time Behavior::CalculateProposingTimestampAfter(const std::int64_t time) const {
  return CalculateProposingTimestamp(time) + m_parameters.block_stake_timestamp_interval_seconds;
}

CAmount Behavior::CalculateBlockReward(const Height height) {
  return m_parameters.reward_function(m_parameters, height);
}

uint256 Behavior::GetGenesisBlockHash() const {
  return m_parameters.genesis_block.hash;
}

const CBlock &Behavior::GetGenesisBlock() const {
  return m_parameters.genesis_block.block;
}

bool Behavior::IsGenesisBlockHash(const uint256 &hash) const {
  return hash == GetGenesisBlockHash();
}

bool Behavior::IsGenesisBlock(const CBlock &block) const {
  return IsGenesisBlockHash(block.GetHash());
}

bool Behavior::IsStakeMature(const blockchain::Depth at_depth) const {
  return at_depth >= m_parameters.stake_maturity;
}

const Parameters &Behavior::GetParameters() const {
  return m_parameters;
}

const Settings &Behavior::GetDefaultSettings() const {
  return m_parameters.default_settings;
}

boost::optional<CPubKey> Behavior::ExtractBlockSigningKey(const CBlock &block) const {
  if (block.vtx.empty()) {
    return boost::none;
  }
  const auto &coinbase_inputs = block.vtx[0]->vin;
  if (coinbase_inputs.size() < 2) {
    return boost::none;
  }
  const auto &witness_stack = coinbase_inputs[1].scriptWitness.stack;
  if (witness_stack.size() < 2) {
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
  const auto &public_key_data = witness_stack[1];
  CPubKey public_key(public_key_data.begin(), public_key_data.end());
  if (!public_key.IsValid()) {
    return boost::none;
  }
  return public_key;
}

std::string Behavior::GetNetworkName() const {
  return std::string(m_parameters.network_name);
}

std::chrono::seconds Behavior::GetBlockStakeTimestampInterval() const {
  return std::chrono::seconds(m_parameters.block_stake_timestamp_interval_seconds);
}

const std::vector<unsigned char> &Behavior::GetBase58Prefix(Base58Type type) const {
  return m_parameters.base58_prefixes[type._to_index()];
}

const std::string &Behavior::GetBech32Prefix() const {
  return m_parameters.bech32_human_readable_prefix;
}

std::unique_ptr<Behavior> Behavior::New(Dependency<::ArgsManager> args) {
  if (args->IsArgSet("-customchainparams")) {
    boost::optional<blockchain::Parameters> custom_parameters =
        ReadCustomParametersFromJsonString(
            args->GetArg("-customchainparams", "{}"),
            blockchain::Parameters::RegTest(),
            [](const std::string &error) {
              LogPrintf("-customchainparams invalid: %s\n", error);
            });
    if (!custom_parameters) {
      throw std::runtime_error("Failed to parse custom chain parameters!");
    }
    return NewFromParameters(*custom_parameters);
  } else if (args->GetBoolArg("-regtest", false)) {
    return NewForNetwork(Network::regtest);
  } else if (args->GetBoolArg("-testnet", false)) {
    return NewForNetwork(Network::test);
  } else {
    return NewForNetwork(Network::main);
  }
}

std::unique_ptr<Behavior> Behavior::NewForNetwork(Network network) {
  switch (network) {
    case Network::main:
      return NewFromParameters(Parameters::MainNet());
    case Network::test:
      return NewFromParameters(Parameters::TestNet());
    case Network::regtest:
      return NewFromParameters(Parameters::RegTest());
  }
  assert(false && "silence gcc warnings");
}

std::unique_ptr<Behavior> Behavior::NewFromParameters(const Parameters &parameters) {
  return MakeUnique<blockchain::Behavior>(parameters);
}

namespace {
//! A global blockchain_behavior instance which is managed outside of the
//! injector as there are parts of united which require access to the currently
//! selected blockchain parameters before and after the injector
std::unique_ptr<Behavior> g_blockchain_behavior;
}  // namespace

void Behavior::MakeGlobal(Dependency<::ArgsManager> args) {
  SetGlobal(New(args));
}

void Behavior::SetGlobal(std::unique_ptr<Behavior> &&behavior) {
  g_blockchain_behavior.swap(behavior);
}

Behavior &Behavior::GetGlobal() {
  assert(g_blockchain_behavior && "global blockchain::Behavior is not initialized");
  return *g_blockchain_behavior;
}

}  // namespace blockchain
