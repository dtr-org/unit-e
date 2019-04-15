// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <rpc/server.h>
#include <script/standard.h>
#include <test/rpc_test_utils.h>
#include <validation.h>
#include <wallet/rpcwalletext.h>
#include <wallet/test/wallet_test_fixture.h>

#include <boost/test/unit_test.hpp>


// Retrieve details about a transaction with specified number of confirmations
static UniValue FindByConfirmations(int confirmations) {
  const UniValue transactions = CallRPC("filtertransactions {\"count\":0}");

  const auto &values = transactions.getValues();
  const auto it = std::find_if(values.begin(), values.end(), [&confirmations](const UniValue &x) {
    return find_value(x, "confirmations").get_int64() == confirmations;
  });

  return *it;
}


BOOST_AUTO_TEST_SUITE(rpcwalletext_tests)

BOOST_FIXTURE_TEST_CASE(genesis_block_coinbase, TestChain100Setup) {
  const UniValue genesis_coinbase = FindByConfirmations(COINBASE_MATURITY + 1);

  BOOST_CHECK_EQUAL(find_value(genesis_coinbase, "category").get_str(), "coinbase");

  // The returned amount should equal the amount credited to us
  BOOST_CHECK_EQUAL(find_value(genesis_coinbase, "amount").get_real(), 10000);
}

BOOST_FIXTURE_TEST_CASE(regular_coinbase, TestChain100Setup) {
  const UniValue regular_coinbase = FindByConfirmations(1);

  BOOST_CHECK_EQUAL(find_value(regular_coinbase, "category").get_str(), "immature");

  // The amount should equal the block reward
  BOOST_CHECK_EQUAL(find_value(regular_coinbase, "amount").get_real(), 3.75);
}

BOOST_AUTO_TEST_SUITE_END()
