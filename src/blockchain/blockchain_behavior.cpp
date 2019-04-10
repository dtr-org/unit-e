// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_behavior.h>

#include <blockchain/blockchain_custom_parameters.h>

namespace blockchain {

namespace {
//! \brief Calculates the absolute minimum serialized size a tx can possibly have.
//!
//! That is: A transaction with one input, one output, and they are all empty.
std::size_t CalculateAbsoluteTransactionSizeMinimum() {
  CMutableTransaction mut_tx;
  mut_tx.vin.emplace_back();
  mut_tx.vout.emplace_back();
  const CTransaction tx(mut_tx);
  return GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS);
}
}  // namespace

Behavior::Behavior(const Parameters &parameters) noexcept
    : m_parameters(parameters), m_absolute_transaction_size_minimum(CalculateAbsoluteTransactionSizeMinimum()) {
  CheckConsistency();
}

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
  const CAmount base_reward = m_parameters.reward_function(m_parameters, height);
  return ufp64::mul_to_uint(m_parameters.immediate_reward_fraction, base_reward);
}

CAmount Behavior::CalculateFinalizationReward(const Height height) {
  const CAmount base_reward = m_parameters.reward_function(m_parameters, height);
  return ufp64::mul_to_uint(m_parameters.finalization_reward_fraction, base_reward);
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

const Parameters &Behavior::GetParameters() const {
  return m_parameters;
}

const Settings &Behavior::GetDefaultSettings() const {
  return m_parameters.default_settings;
}

std::size_t Behavior::GetTransactionWeight(const CTransaction &tx) const {
  return ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (m_parameters.witness_scale_factor - 1) + ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
}

std::size_t Behavior::GetBlockWeight(const CBlock &block) const {
  return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (m_parameters.witness_scale_factor - 1) + ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
}

std::size_t Behavior::GetTransactionInputWeight(const CTxIn &txin) const {
  // scriptWitness size is added here because witnesses and txins are split up in segwit serialization.
  return ::GetSerializeSize(txin, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (m_parameters.witness_scale_factor - 1) + ::GetSerializeSize(txin, SER_NETWORK, PROTOCOL_VERSION) + ::GetSerializeSize(txin.scriptWitness.stack, SER_NETWORK, PROTOCOL_VERSION);
}

bool Behavior::IsInMoneyRange(const CAmount amount) const {
  return amount >= 0 && amount <= m_parameters.expected_maximum_supply;
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

std::size_t Behavior::GetAbsoluteTransactionSizeMinimum() const {
  return m_absolute_transaction_size_minimum;
}

std::unique_ptr<Behavior> Behavior::New(Dependency<::ArgsManager> args) {
  // We assume that the args were sanitized by InitParameterInteraction
  if (args->IsArgSet("-customchainparamsfile")) {
    blockchain::Parameters custom_parameters = ReadCustomParametersFromFile(
        args->GetArg("-customchainparamsfile", ""),
        blockchain::Parameters::RegTest());
    return NewFromParameters(custom_parameters);
  } else if (args->IsArgSet("-customchainparams")) {
    blockchain::Parameters custom_parameters = ReadCustomParametersFromJsonString(
        args->GetArg("-customchainparams", "{}"),
        blockchain::Parameters::RegTest());
    return NewFromParameters(custom_parameters);
  } else if (args->GetBoolArg("-regtest", false)) {
    return NewForNetwork(Network::regtest);
  }
  return NewForNetwork(Network::test);
}

std::unique_ptr<Behavior> Behavior::NewForNetwork(Network network) {
  switch (network) {
    case Network::test:
      return NewFromParameters(Parameters::TestNet());
    case Network::regtest:
      return NewFromParameters(Parameters::RegTest());
  }
  assert(!"silence gcc warnings");
}

std::unique_ptr<Behavior> Behavior::NewFromParameters(const Parameters &parameters) {
  return MakeUnique<blockchain::Behavior>(parameters);
}

namespace {
//! A global blockchain_behavior instance which is managed outside of the
//! injector as there are parts of unit-e which require access to the currently
//! selected blockchain parameters before and after the injector
std::unique_ptr<Behavior> g_blockchain_behavior;
}  // namespace

Dependency<blockchain::Behavior> Behavior::MakeGlobal(Dependency<::ArgsManager> args) {
  SetGlobal(New(args));
  return g_blockchain_behavior.get();
}

void Behavior::SetGlobal(std::unique_ptr<Behavior> &&behavior) {
  g_blockchain_behavior.swap(behavior);
}

Behavior &Behavior::GetGlobal() {
  assert(g_blockchain_behavior && "global blockchain::Behavior is not initialized");
  return *g_blockchain_behavior;
}

void Behavior::CheckConsistency() const {
  assert(this->m_parameters.stake_maturity_activation_height >= this->m_parameters.stake_maturity &&
         "Invalid blockchain parameters: 'stake_maturity_activation_height' "
         "must be greater or equal 'stake_maturity'");
}

}  // namespace blockchain
