// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_parameters.h>

#include <blockchain/blockchain_genesis.h>
#include <utilstrencodings.h>
#include <numeric>

namespace blockchain {

namespace {

constexpr const char *MAIN_NET_NAME = "main";
constexpr const char *TEST_NET_NAME = "test";
constexpr const char *REGTEST_NET_NAME = "regtest";

Parameters BuildMainNetParameters() {
  Parameters p{};  // designated initializers would be so nice here
  p.network_name = MAIN_NET_NAME;
  p.block_stake_timestamp_interval_seconds = 16;
  p.block_time_seconds = 16;
  p.max_future_block_time_seconds = 2 * 60 * 60;
  p.relay_non_standard_transactions = false;
  p.mine_blocks_on_demand = false;
  p.maximum_block_size = 1000000;
  p.maximum_block_weight = 4000000;
  p.maximum_block_serialized_size = 4000000;
  p.maximum_block_sigops_cost = 80000;
  p.coinbase_maturity = 100;
  p.stake_maturity = 200;
  p.initial_supply = 150000000000000000;
  p.reward_schedule = {3750000000, 1700000000, 550000000, 150000000, 31000000};
  p.period_blocks = 19710000;
  p.maximum_supply = 2718275100 * UNIT;  // e billion UTE
  assert(p.maximum_supply == p.initial_supply + std::accumulate(p.reward_schedule.begin(), p.reward_schedule.end(), CAmount(0)) * p.period_blocks);
  p.reward_function = [](const Parameters &p, Height h) -> CAmount {
    const std::uint32_t period = h / p.period_blocks;
    if (period >= p.reward_schedule.size()) {
      return 0;
    }
    return p.reward_schedule[period];
  };
  p.difficulty_function = [](const Parameters &p, Height h, ChainAccess &chain) -> Difficulty {
    // UNIT-E: Does not adjust difficulty for now
    const auto tip = chain.AtDepth(1);
    return tip->nBits;
  };

  // The message start string is designed to be unlikely to occur in normal data.
  // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
  // a large 32-bit integer with any alignment. They are different from bitcoin.
  p.message_start_characters[0] = 0xee;
  p.message_start_characters[1] = 0xee;
  p.message_start_characters[2] = 0xae;
  p.message_start_characters[3] = 0xc1;

  p.base58_prefixes[Base58Type::PUBKEY_ADDRESS] = {0x00};
  p.base58_prefixes[Base58Type::SCRIPT_ADDRESS] = {0x05};
  p.base58_prefixes[Base58Type::SECRET_KEY] = {0x80};
  p.base58_prefixes[Base58Type::EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
  p.base58_prefixes[Base58Type::EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

  p.bech32_human_readable_prefix = "ue";

  p.deployment_confirmation_period = 2016;
  p.rule_change_activation_threshold = 1916;

  static GenesisBlock genesisBlock{
      GenesisBlockBuilder().Add(MainnetFunds()).Build(p)};

  p.genesis_block = &genesisBlock;

  return p;
}

Parameters BuildTestNetParameters() {
  Parameters p = Parameters::MainNet();
  p.network_name = TEST_NET_NAME;
  p.relay_non_standard_transactions = true;
  p.coinbase_maturity = 10;
  p.stake_maturity = 20;

  p.message_start_characters[0] = 0xfd;
  p.message_start_characters[1] = 0xfc;
  p.message_start_characters[2] = 0xfb;
  p.message_start_characters[3] = 0xfa;

  p.base58_prefixes[Base58Type::PUBKEY_ADDRESS] = {0x6F};
  p.base58_prefixes[Base58Type::SCRIPT_ADDRESS] = {0xC4};
  p.base58_prefixes[Base58Type::SECRET_KEY] = {0xEF};
  p.base58_prefixes[Base58Type::EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
  p.base58_prefixes[Base58Type::EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

  p.bech32_human_readable_prefix = "tue";

  static GenesisBlock genesisBlock{
      GenesisBlockBuilder().Add(TestnetFunds()).Build(p)};
  p.genesis_block = &genesisBlock;

  return p;
}

Parameters BuildRegTestParameters() {
  Parameters p = Parameters::MainNet();
  p.network_name = REGTEST_NET_NAME;
  p.mine_blocks_on_demand = true;
  p.coinbase_maturity = 1;
  p.stake_maturity = 2;

  p.message_start_characters[0] = 0xfa;
  p.message_start_characters[1] = 0xbf;
  p.message_start_characters[2] = 0xb5;
  p.message_start_characters[3] = 0xda;

  p.base58_prefixes[Base58Type::PUBKEY_ADDRESS] = {0x6F};
  p.base58_prefixes[Base58Type::SCRIPT_ADDRESS] = {0xC4};
  p.base58_prefixes[Base58Type::SECRET_KEY] = {0xEF};
  p.base58_prefixes[Base58Type::EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
  p.base58_prefixes[Base58Type::EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

  p.bech32_human_readable_prefix = "uert";

  static GenesisBlock genesisBlock{
      GenesisBlockBuilder().Add(RegtestFunds()).Build(p)};
  p.genesis_block = &genesisBlock;

  return p;
}

}  // namespace

const Parameters &Parameters::MainNet() noexcept {
  static Parameters parameters = BuildMainNetParameters();
  return parameters;
}

const Parameters &Parameters::TestNet() noexcept {
  static Parameters parameters = BuildTestNetParameters();
  return parameters;
}

const Parameters &Parameters::RegTest() noexcept {
  static Parameters parameters = BuildRegTestParameters();
  return parameters;
}

const blockchain::Network Parameters::GetNetwork() const {
  if (network_name == MAIN_NET_NAME) {
    return Network::main;
  } else if (network_name == TEST_NET_NAME) {
    return Network::test;
  } else if (network_name == REGTEST_NET_NAME) {
    return Network::regtest;
  }
  assert(false && "Unknown network type");
}

GenesisBlock::GenesisBlock(const CBlock &block)
    : block(block), hash(block.GetHash()) {}

}  // namespace blockchain
