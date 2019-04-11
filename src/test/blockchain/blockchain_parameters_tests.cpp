// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_parameters.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

using namespace blockchain;

BOOST_FIXTURE_TEST_SUITE(blockchain_parameters_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(total_supply_test) {

  Parameters params = blockchain::Parameters::TestNet();

  constexpr CAmount initial_supply = 150000000000000000;
  constexpr CAmount max_supply = initial_supply + (3750000000 + 1700000000 + 550000000 + 150000000 + 31000000) * 1971000 * 10;
  BOOST_CHECK_EQUAL(max_supply, params.maximum_supply);

  BOOST_CHECK(MoneyRange(max_supply));
  BOOST_CHECK(!MoneyRange(max_supply + 1));

  constexpr CAmount theoretic_supply = 2718281828 * UNIT;  // e billion tokens
  constexpr CAmount expected_delta = 6728 * UNIT;
  BOOST_CHECK_EQUAL(params.maximum_supply, theoretic_supply - expected_delta);
}

BOOST_AUTO_TEST_CASE(reward_function_test) {

  Parameters params = blockchain::Parameters::TestNet();

  CAmount block_reward = params.reward_function(params, 0);
  BOOST_CHECK_EQUAL(375000000, block_reward);

  BOOST_CHECK_EQUAL(375000000, block_reward);

  block_reward = params.reward_function(params, params.period_blocks - 1);
  BOOST_CHECK_EQUAL(375000000, block_reward);

  block_reward = params.reward_function(params, params.period_blocks);
  BOOST_CHECK_EQUAL(170000000, block_reward);

  block_reward = params.reward_function(params, params.period_blocks * 2);
  BOOST_CHECK_EQUAL(55000000, block_reward);

  block_reward = params.reward_function(params, params.period_blocks * 3);
  BOOST_CHECK_EQUAL(15000000, block_reward);

  block_reward = params.reward_function(params, params.period_blocks * 4);
  BOOST_CHECK_EQUAL(3100000, block_reward);

  block_reward = params.reward_function(params, params.period_blocks * 5);
  BOOST_CHECK_EQUAL(0, block_reward);

  block_reward = params.reward_function(params, params.period_blocks * 500);
  BOOST_CHECK_EQUAL(0, block_reward);
}

class ActiveChainWithTime : public ChainAccess {
 public:
  ActiveChainWithTime(const CBlock &genesis) {
    m_chain.emplace_back(MakeUnique<CBlockIndex>(genesis));
  }

  const CBlockIndex *AtDepth(const Depth depth) const override {
    return m_chain[m_chain.size() - depth].get();
  }

  const CBlockIndex *AtHeight(const Height height) const override {
    return m_chain[height].get();
  }

  void Append(const Difficulty difficulty,
              const Time time_taken_to_mine) {

    const CBlockIndex *prev_index = m_chain.back().get();

    auto index = MakeUnique<CBlockIndex>();
    index->nBits = difficulty;
    index->nTime = prev_index->nTime + time_taken_to_mine;

    m_chain.emplace_back(std::move(index));
  }

 private:
  std::vector<std::unique_ptr<CBlockIndex>> m_chain;
};

BOOST_AUTO_TEST_CASE(generic_difficulty_function_test) {

  Parameters params = Parameters::TestNet();
  params.difficulty_adjustment_window = 128;

  const CBlock &genesis = params.genesis_block.block;

  ActiveChainWithTime chain(genesis);

  Height h = 0;

  BOOST_CHECK_EQUAL(genesis.nBits, params.difficulty_function(params, h, chain));

  {
    // Ideal block time => no change in difficulty
    ++h;
    const Difficulty difficulty_before = params.difficulty_function(params, h, chain);
    for (; h < 250; ++h) {
      chain.Append(params.difficulty_function(params, h, chain), params.block_time_seconds);
    }
    const Difficulty difficulty_after = params.difficulty_function(params, h, chain);
    BOOST_CHECK_EQUAL(difficulty_before, difficulty_after);
  }

  {
    // block time decreases => difficulty value should decrease
    const Difficulty difficulty_before = params.difficulty_function(params, h, chain);
    for (; h < 500; ++h) {
      chain.Append(params.difficulty_function(params, h, chain), params.block_time_seconds - 1);
    }
    const Difficulty difficulty_after = params.difficulty_function(params, h, chain);
    BOOST_CHECK_LT(difficulty_after, difficulty_before);
  }

  {
    // Ideal block time => difficulty should not change
    // However need to give it time to settle
    for (; h < 1000; ++h) {
      chain.Append(params.difficulty_function(params, h, chain), params.block_time_seconds);
    }

    const Difficulty difficulty_before = params.difficulty_function(params, h, chain);
    for (; h < 1250; ++h) {
      chain.Append(params.difficulty_function(params, h, chain), params.block_time_seconds);
    }

    const Difficulty difficulty_after = params.difficulty_function(params, h, chain);
    BOOST_CHECK_EQUAL(difficulty_before, difficulty_after);
  }

  {
    // block time increases => difficulty value should increase
    const Difficulty difficulty_before = params.difficulty_function(params, h, chain);
    for (; h < 1500; ++h) {
      chain.Append(params.difficulty_function(params, h, chain), params.block_time_seconds + 1);
    }
    const Difficulty difficulty_after = params.difficulty_function(params, h, chain);
    BOOST_CHECK_GT(difficulty_after, difficulty_before);
  }
}

BOOST_AUTO_TEST_CASE(difficulty_function_max_test) {

  const uint256 max_difficulty_value = uint256S("000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
  const arith_uint256 almost_max_difficulty_value = UintToArith256(max_difficulty_value) - 1;

  Parameters params = Parameters::TestNet();
  params.max_difficulty_value = max_difficulty_value;
  CBlock &genesis = params.genesis_block.block;
  genesis.nBits = almost_max_difficulty_value.GetCompact();

  ActiveChainWithTime chain(genesis);

  Height h = 1;
  // During the window should return difficulty of the genesis no matter what
  for (; h < params.difficulty_adjustment_window; ++h) {
    const Difficulty new_difficulty = params.difficulty_function(params, h, chain);
    chain.Append(new_difficulty, params.block_time_seconds * 2);
    BOOST_CHECK_EQUAL(almost_max_difficulty_value.GetCompact(), new_difficulty);
  }

  // After window, should adjust difficulty and hit the max value
  for (; h < 2 * params.difficulty_adjustment_window; ++h) {
    const Difficulty new_difficulty = params.difficulty_function(params, h, chain);
    chain.Append(new_difficulty, params.block_time_seconds * 2);
    BOOST_CHECK_EQUAL(UintToArith256(max_difficulty_value).GetCompact(), new_difficulty);
  }
}

BOOST_AUTO_TEST_SUITE_END()
