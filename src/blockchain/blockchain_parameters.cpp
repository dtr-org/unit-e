// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_parameters.h>

#include <blockchain/blockchain_genesis.h>
#include <utilstrencodings.h>

namespace blockchain {

namespace {

Parameters BuildMainNetParameters() {
  Parameters p{};  // designated initializers would be so nice here
  p.network_name = "main";
  p.block_stake_timestamp_interval_seconds = 16;
  p.block_time_seconds = 16;
  p.relay_non_standard_transactions = false;
  p.mine_blocks_on_demand = false;
  p.maximum_block_size = 1000000;
  p.maximum_block_weight = 4000000;
  p.maximum_block_serialized_size = 4000000;
  p.maximum_block_sigops_cost = 80000;
  p.coinbase_maturity = 100;
  p.restake_maturity = 200;
  p.maximum_supply = 2718281828 * UNIT; // e billion UTE
  p.reward_function = [](const Parameters &p, MoneySupply s, Height h) -> CAmount {
    // UNIT-E: This reward function is not here to stay, it is just some simple reward function as in particl
    constexpr uint64_t secondsInAYear = 365 * 24 * 60 * 60;
    // 2 percent inflation (2% of current money supply distributed over all blocks in a year)
    return (s * 2 / 100) / (secondsInAYear / p.block_stake_timestamp_interval_seconds);
  };
  p.message_start_characters[0] = 0xee;
  p.message_start_characters[1] = 0xee;
  p.message_start_characters[2] = 0xae;
  p.message_start_characters[3] = 0xc1;
  p.deployment_confirmation_period = 2016;
  p.rule_change_activation_threshold = 1916;

  static GenesisBlock genesisBlock {
    GenesisBlockBuilder()
        .Add(Funds{
            P2WPKH(10000 * UNIT, "33a471b2c4d3f45b9ab4707455f7d2e917af5a6e"),
            P2WPKH(10000 * UNIT, "7eac29a2e24c161e2d18d8d1249a6327d18d390f"),
            P2WPKH(10000 * UNIT, "caca901140bf287eff2af36edeb48503cec4eb9f"),
            P2WPKH(10000 * UNIT, "1f34ea7e96d82102b22afed6d53d02715f8f6621"),
            P2WPKH(10000 * UNIT, "eb07ad5db790ee4324b5cdd635709f47e41fd867")})
        .Build(p)
  };
  p.genesis_block = &genesisBlock;

  return p;
}

Parameters BuildTestNetParameters() {
  Parameters p = Parameters::MainNet();
  p.network_name = "test";
  p.relay_non_standard_transactions = true;
  p.coinbase_maturity = 10;
  p.restake_maturity = 20;
  p.message_start_characters[0] = 0xfd;
  p.message_start_characters[1] = 0xfc;
  p.message_start_characters[2] = 0xfb;
  p.message_start_characters[3] = 0xfa;

  static GenesisBlock genesisBlock {
    GenesisBlockBuilder()
        .Add(Funds{
            P2WPKH(10000 * UNIT, "33a471b2c4d3f45b9ab4707455f7d2e917af5a6e"),
            P2WPKH(10000 * UNIT, "7eac29a2e24c161e2d18d8d1249a6327d18d390f"),
            P2WPKH(10000 * UNIT, "caca901140bf287eff2af36edeb48503cec4eb9f"),
            P2WPKH(10000 * UNIT, "1f34ea7e96d82102b22afed6d53d02715f8f6621"),
            P2WPKH(10000 * UNIT, "eb07ad5db790ee4324b5cdd635709f47e41fd867")})
        .Build(p)
  };
  p.genesis_block = &genesisBlock;

  return p;
}

Parameters BuildRegTestParameters() {
  Parameters p = Parameters::MainNet();
  p.network_name = "regtest";
  p.mine_blocks_on_demand = true;
  p.coinbase_maturity = 1;
  p.restake_maturity = 2;
  p.message_start_characters[0] = 0xfa;
  p.message_start_characters[1] = 0xbf;
  p.message_start_characters[2] = 0xb5;
  p.message_start_characters[3] = 0xda;

  static GenesisBlock genesisBlock {
    GenesisBlockBuilder()
        .Add(Funds{
            P2WPKH(10000 * UNIT, "33a471b2c4d3f45b9ab4707455f7d2e917af5a6e"),
            P2WPKH(10000 * UNIT, "7eac29a2e24c161e2d18d8d1249a6327d18d390f"),
            P2WPKH(10000 * UNIT, "caca901140bf287eff2af36edeb48503cec4eb9f"),
            P2WPKH(10000 * UNIT, "1f34ea7e96d82102b22afed6d53d02715f8f6621"),
            P2WPKH(10000 * UNIT, "eb07ad5db790ee4324b5cdd635709f47e41fd867")})
        .Build(p)
  };
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

GenesisBlock::GenesisBlock(const CBlock &block)
    : block(block), hash(block.GetHash()) {}

}  // namespace blockchain
