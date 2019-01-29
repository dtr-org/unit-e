// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_parameters.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

using namespace blockchain;

bool BlockRewardEquals(const BlockReward &a, const BlockReward &b) {
  return a.finalization_reward == b.finalization_reward &&
      a.immediate_reward == b.immediate_reward &&
      a.validator_funds == b.validator_funds;
}

BOOST_FIXTURE_TEST_SUITE(blockchain_parameters_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(total_supply_test) {

  Parameters params = blockchain::Parameters::TestNet();

  constexpr CAmount initial_supply = 150000000000000000;
  constexpr CAmount max_supply = initial_supply + (3750000000 + 1700000000 + 550000000 + 150000000 + 31000000) * 1971000 * 10;
  BOOST_CHECK_EQUAL(max_supply, params.maximum_supply);

  constexpr CAmount theoretic_supply = 2718281828 * UNIT;  // e billion tokens
  constexpr CAmount expected_delta = 6728 * UNIT;
  BOOST_CHECK_EQUAL(params.maximum_supply, theoretic_supply - expected_delta);
}

BOOST_AUTO_TEST_CASE(reward_function_test) {

  const Parameters params = blockchain::Parameters::MainNet();

  auto test_reward = [params](int period) -> BlockReward {
    auto base_reward = static_cast<uint64_t>(params.reward_schedule[period]);
    return {static_cast<int64_t>(ufp64::mul_to_uint(ufp64::div_2uint(1, 10), base_reward)),
            static_cast<int64_t>(ufp64::mul_to_uint(ufp64::div_2uint(4, 10), base_reward)),
            static_cast<int64_t>(ufp64::mul_to_uint(ufp64::div_2uint(5, 10), base_reward))};
  };

  BlockReward block_reward = params.reward_function(params, 0);
  BlockReward expected_reward = test_reward(0);
  BOOST_CHECK(BlockRewardEquals(expected_reward, block_reward));

  block_reward = params.reward_function(params, 1000);
  BOOST_CHECK(BlockRewardEquals(expected_reward, block_reward));

  block_reward = params.reward_function(params, params.period_blocks - 1);
  BOOST_CHECK(BlockRewardEquals(expected_reward, block_reward));

  block_reward = params.reward_function(params, params.period_blocks);
  expected_reward = test_reward(1);
  BOOST_CHECK(BlockRewardEquals(expected_reward, block_reward));

  block_reward = params.reward_function(params, params.period_blocks * 2);
  expected_reward = test_reward(2);
  BOOST_CHECK(BlockRewardEquals(expected_reward, block_reward));

  block_reward = params.reward_function(params, params.period_blocks * 3);
  expected_reward = test_reward(3);
  BOOST_CHECK(BlockRewardEquals(expected_reward, block_reward));

  block_reward = params.reward_function(params, params.period_blocks * 4);
  expected_reward = test_reward(4);
  BOOST_CHECK(BlockRewardEquals(expected_reward, block_reward));

  block_reward = params.reward_function(params, params.period_blocks * 5);
  BOOST_CHECK(BlockRewardEquals({0,0,0}, block_reward));

  block_reward = params.reward_function(params, params.period_blocks * 500);
  BOOST_CHECK(BlockRewardEquals({0,0,0}, block_reward));
}

BOOST_AUTO_TEST_SUITE_END()
