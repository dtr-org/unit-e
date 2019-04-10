// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_parameters.h>

#include <blockchain/blockchain_genesis.h>
#include <utilstrencodings.h>

#include <numeric>

namespace blockchain {

Parameters Parameters::Base() noexcept {
  Parameters p{};  // designated initializers would be so nice here

  p.block_stake_timestamp_interval_seconds = 4;
  p.block_time_seconds = 16;
  p.max_future_block_time_seconds = 15;
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
  p.immediate_reward_fraction = ufp64::div_2uint(1, 10);
  p.period_blocks = 19710000;
  p.maximum_supply = 2718275100 * UNIT;  // e billion UTE
  assert(p.maximum_supply == p.initial_supply + std::accumulate(p.reward_schedule.begin(), p.reward_schedule.end(), CAmount(0)) * p.period_blocks);
  p.reward_function = [](const Parameters &p, Height h) -> CAmount {
    const uint64_t period = h / p.period_blocks;

    CAmount base_reward = 0;

    if (period < p.reward_schedule.size()) {
      base_reward = static_cast<uint64_t>(p.reward_schedule[period]);
    }
    return ufp64::mul_to_uint(p.immediate_reward_fraction, base_reward);
  };

  p.difficulty_adjustment_window = 128;
  p.max_difficulty_value = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

  p.difficulty_function = [](const Parameters &p, Height height, ChainAccess &chain) -> Difficulty {
    const arith_uint256 max_difficulty_value = UintToArith256(p.max_difficulty_value);

    if (height <= p.difficulty_adjustment_window) {
      return max_difficulty_value.GetCompact();
    }

    const Height window_end = height - 1;
    const Height window_start = height - 1 - p.difficulty_adjustment_window;

    const CBlockIndex *end_index = chain.AtHeight(window_end);
    const CBlockIndex *start_index = chain.AtHeight(window_start);

    if (end_index->nTime <= start_index->nTime) {
      return max_difficulty_value.GetCompact();
    }

    const blockchain::Time actual_window_duration = end_index->nTime - start_index->nTime;

    arith_uint256 window_difficulties_sum;
    for (blockchain::Height i = window_start + 1; i <= window_end; ++i) {
      arith_uint256 temp;
      temp.SetCompact(chain.AtHeight(i)->nBits);
      window_difficulties_sum += temp;
    }

    const arith_uint256 avg_difficulty = window_difficulties_sum / p.difficulty_adjustment_window;
    const arith_uint256 numerator = actual_window_duration * avg_difficulty;
    if (numerator / actual_window_duration != avg_difficulty) {
      // Overflow
      return max_difficulty_value.GetCompact();
    }

    const blockchain::Time expected_window_duration = p.difficulty_adjustment_window * p.block_time_seconds;
    const arith_uint256 next_difficulty = numerator / expected_window_duration;

    if (next_difficulty > max_difficulty_value) {
      return max_difficulty_value.GetCompact();
    }

    return next_difficulty.GetCompact();
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

  p.default_settings.finalizer_vote_from_epoch_block_number = 35;

  return p;
}

Parameters Parameters::TestNet() noexcept {
  Parameters p = Parameters::Base();
  p.network_name = "test";

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

  p.genesis_block = GenesisBlock(GenesisBlockBuilder().Add(TestnetFunds()).Build(p));

  p.default_settings.p2p_port = 17182;
  p.data_dir_suffix = "testnet";

  return p;
}

Parameters Parameters::RegTest() noexcept {
  Parameters p = Parameters::Base();
  p.network_name = "regtest";

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

  p.genesis_block = GenesisBlock(GenesisBlockBuilder().SetTime(1296688602).SetBits(0x207fffff).Add(RegtestFunds()).Build(p));

  p.default_settings.node_is_proposer = false;
  p.default_settings.stake_split_threshold = 1000 * UNIT;
  p.default_settings.p2p_port = 17292;
  p.default_settings.finalizer_vote_from_epoch_block_number = 1;
  p.data_dir_suffix = "regtest";

  p.difficulty_adjustment_window = 0;
  p.max_difficulty_value = uint256::zero;
  p.difficulty_function = [](const Parameters &p, Height height, ChainAccess &chain) -> Difficulty {
    return p.genesis_block.block.nBits;
  };

  return p;
}

GenesisBlock::GenesisBlock() noexcept : block(), hash(block.GetHash()) {}

GenesisBlock::GenesisBlock(const CBlock &block) noexcept : block(block), hash(block.GetHash()) {}

}  // namespace blockchain
