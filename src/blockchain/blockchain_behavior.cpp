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

namespace {

//! \brief Extract the staking key from a P2WPKH witness stack.
//!
//! As per https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki#p2wpkh,
//! a P2WPKH transaction looks like this:
//!
//!    witness:      <signature> <pubkey>
//!    scriptSig:    (empty)
//!    scriptPubKey: 0 <20-byte-key-hash>
//!                  (0x0014{20-byte-key-hash})
//!
//! That is: The pubkey we're interested in is in stack[1]
//! (stack[0] is the signature).
std::vector<CPubKey> ExtractP2WPKHKeys(const std::vector<std::vector<unsigned char>> &witness_stack) {
  if (witness_stack.size() != 2) {
    return std::vector<CPubKey>{};
  }
  const std::vector<unsigned char> &public_key_data = witness_stack[1];
  CPubKey public_key(public_key_data.begin(), public_key_data.end());
  if (!public_key.IsFullyValid()) {
    return std::vector<CPubKey>{};
  }
  return std::vector<CPubKey>{public_key};
}

//! \brief Extract the staking key from a P2WSH witness stack.
//!
//! As per https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki#p2wsh,
//! a P2WSH transaction looks like this:
//!
//!    witness:      0 <signature1> <1 <pubkey1> <pubkey2> 2 CHECKMULTISIG>
//!    scriptSig:    (empty)
//!    scriptPubKey: 0 <32-byte-hash>
//!                  (0x0020{32-byte-hash})
//!
//! "0" in the first witness item is actually empty (each item in the
//! stack is encoded using a var int and the data following, this item
//! will be encoded using the var int 0 with no data following).
//!
//! The script is just an example and it is serialized. So we need to
//! pop the script off the stack, deserialize it, and check what kind
//! of script it is in order to extract the signing key.
std::vector<CPubKey> ExtractP2WSHKeys(const std::vector<std::vector<unsigned char>> &witness_stack) {

  if (witness_stack.empty()) {
    return std::vector<CPubKey>{};
  }
  const std::vector<unsigned char> &script_data = witness_stack.back();
  if (script_data.empty()) {
    return std::vector<CPubKey>{};
  }
  const CScript witness_script(script_data.begin(), script_data.end());
  txnouttype type;
  std::vector<std::vector<unsigned char>> solutions;
  if (Solver(witness_script, type, solutions)) {
    switch (type) {
      case TX_PUBKEYHASH: {
        if (witness_stack.size() < 2) {
          return std::vector<CPubKey>{};
        }
        const CPubKey pub_key(witness_stack[1]);
        return std::vector<CPubKey>{pub_key};
      }
      case TX_PUBKEY: {
        const CPubKey pub_key(solutions[0]);
        return std::vector<CPubKey>{pub_key};
      }
      case TX_MULTISIG: {
        std::vector<CPubKey> result;
        // the first solution contains an OP_SMALLINTEGER with the number of signatures required
        const auto num_signatures = static_cast<std::uint8_t>(solutions.front()[0]);
        if (num_signatures != 1) {
          // stake is signed by a single proposer only and the block carries a single
          // signature of that proposer. 2-of-3 and similar multisig scenarios are not
          // allowed for staking.
          return std::vector<CPubKey>{};
        }
        // the last solution contains an OP_SMALLINTEGER with the number of pubkeys provided
        const auto num_pubkeys = static_cast<std::uint8_t>(solutions.back()[0]);
        if (num_pubkeys != solutions.size() - 2) {
          // number of pubkeys provided does not match amount required.
          return std::vector<CPubKey>{};
        }
        for (std::size_t i = 1; i < solutions.size() - 1; ++i) {
          const CPubKey key(solutions[i]);
          if (!key.IsFullyValid()) {
            continue;
          }
          result.emplace_back(solutions[i]);
        }
        return result;
      }
      default:
        return std::vector<CPubKey>{};
    }
  }
  return std::vector<CPubKey>{};
}

}  // namespace

std::vector<CPubKey> Behavior::ExtractBlockSigningKeys(const CTxIn &input) const {
  const std::vector<std::vector<unsigned char>> &witness_stack = input.scriptWitness.stack;
  std::vector<CPubKey> p2wpkh_keys = ExtractP2WPKHKeys(witness_stack);
  if (p2wpkh_keys.size() == 1) {
    // p2wpkh should yield one key only.
    return p2wpkh_keys;
  }
  return ExtractP2WSHKeys(witness_stack);
}

std::vector<CPubKey> Behavior::ExtractBlockSigningKeys(const CBlock &block) const {
  if (block.vtx.empty()) {
    return std::vector<CPubKey>{};
  }
  const std::vector<CTxIn> &coinbase_inputs = block.vtx[0]->vin;
  if (coinbase_inputs.size() < 2) {
    return std::vector<CPubKey>{};
  }
  return ExtractBlockSigningKeys(coinbase_inputs[1]);
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
    blockchain::Parameters custom_parameters = ReadCustomParametersFromJsonString(
        args->GetArg("-customchainparams", "{}"),
        blockchain::Parameters::RegTest());
    return NewFromParameters(custom_parameters);
  } else if (args->GetBoolArg("-regtest", false)) {
    return NewForNetwork(Network::regtest);
  } else if (args->GetBoolArg("-testnet", false)) {
    return NewForNetwork(Network::test);
  }
  return NewForNetwork(Network::main);
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
