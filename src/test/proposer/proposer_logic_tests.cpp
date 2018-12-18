// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/multiwallet.h>
#include <proposer/proposer.h>
#include <test/test_unite.h>
#include <wallet/wallet.h>
#include <boost/test/unit_test.hpp>

#include <thread>

#if defined(__GNUG__) and not defined(__clang__)
// Fakeit does not work with GCC's devirtualization
// which is enabled with -O2 (the default) or higher.
#pragma GCC optimize("no-devirtualize")
#endif

#include <test/fakeit/fakeit.hpp>

BOOST_AUTO_TEST_SUITE(proposer_tests)

fakeit::Mock<staking::Network> networkMock;
fakeit::Mock<staking::ActiveChain> chainMock;

Dependency<staking::Network> network = &networkMock.get();
Dependency<staking::ActiveChain> chain = &chainMock.get();

BOOST_AUTO_TEST_SUITE_END()
