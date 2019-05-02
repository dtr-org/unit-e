// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_behavior.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

template <class T, std::size_t N>
constexpr std::size_t size(const T (&array)[N]) noexcept {
  return N;
}

BOOST_FIXTURE_TEST_SUITE(blockchain_behavior_test, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(creation_test) {
  ArgsManager args;

  {
    const char *const params[] = {"unit-e"};
    args.ParseParameters(size(params), params);
    const auto b = blockchain::Behavior::New(&args);
    BOOST_CHECK_EQUAL(b->GetNetworkName(), "test");
  }

  {
    const char *const params[] = {"unit-e", "-regtest"};
    args.ParseParameters(size(params), params);
    const auto b = blockchain::Behavior::New(&args);
    BOOST_CHECK_EQUAL(b->GetNetworkName(), "regtest");
  }

  {
    const char *const params[] = {"unit-e", "-regtest=0"};
    args.ParseParameters(size(params), params);
    const auto b = blockchain::Behavior::New(&args);
    BOOST_CHECK_EQUAL(b->GetNetworkName(), "test");
  }
}

BOOST_AUTO_TEST_SUITE_END()
