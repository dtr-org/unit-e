// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/coin.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

#include <array>

BOOST_AUTO_TEST_SUITE(coin_tests)

struct Fixture {
  const std::array<uint256, 2> txids{
      {uint256S("01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b"),
       uint256S("682a09fbfaf947a7a385c799bf1eb29ebb1c5ba4880cdf17a291a614740fccf3")}};

  const CScript script;

  const CBlockIndex block1 = [] {
    CBlockIndex i;
    i.nHeight = 1849301;
    return i;
  }();

  const CBlockIndex block2 = [&] {
    CBlockIndex i;
    i.nHeight = block1.nHeight - 1; // older than block1
    return i;
  }();

  const std::array<staking::Coin, 5> coins{
      {staking::Coin(&block1, {txids[0], 0}, {10000, script}),
       staking::Coin(&block1, {txids[0], 1}, {10001, script}),
       staking::Coin(&block2, {txids[0], 2}, {10000, script}),
       staking::Coin(&block1, {txids[0], 3}, {10000, script}),
       staking::Coin(&block1, {txids[1], 3}, {10000, script})}};
};

BOOST_FIXTURE_TEST_CASE(comparator_tests, Fixture) {

  staking::CoinByAmountComparator comparator;

  BOOST_CHECK_MESSAGE(comparator(coins[1], coins[0]), "coins[1] comes before coins[0] because of higher amount");
  BOOST_CHECK_MESSAGE(comparator(coins[2], coins[0]), "coins[2] comes before coins[0] because it is older");
  BOOST_CHECK_MESSAGE(comparator(coins[0], coins[3]), "coins[0] comes before coins[3] because it has the lower vout");
  BOOST_CHECK_MESSAGE(comparator(coins[0], coins[4]), "coins[0] comes before coins[4] because it has the lower txid");
  BOOST_CHECK_MESSAGE(comparator(coins[3], coins[4]), "coins[3] comes before coins[4] because it has the lower txid");

  // check the reverse
  BOOST_CHECK(!comparator(coins[0], coins[1]));
  BOOST_CHECK(!comparator(coins[0], coins[2]));
  BOOST_CHECK(!comparator(coins[3], coins[0]));
  BOOST_CHECK(!comparator(coins[4], coins[0]));
  BOOST_CHECK(!comparator(coins[4], coins[3]));
}

BOOST_FIXTURE_TEST_CASE(coinset_tests, Fixture) {

  staking::CoinSet coin_set;

  for (const staking::Coin &coin : coins) {
    coin_set.insert(coin);
  }

  std::vector<staking::Coin> expected_order{coins[1], coins[2], coins[0], coins[3], coins[4]};
  std::vector<staking::Coin> resulting_order;
  for (const staking::Coin &coin : coin_set) {
    resulting_order.emplace_back(coin);
  }
  BOOST_CHECK_EQUAL(resulting_order, expected_order);
  CAmount prev_amount = resulting_order[0].GetAmount();

  // check that the coins are sorted by amount, descending
  for (const staking::Coin &coin : resulting_order) {
    BOOST_CHECK_GE(prev_amount, coin.GetAmount());
    prev_amount = coin.GetAmount();
  }
}

BOOST_AUTO_TEST_SUITE_END()
