// Copyright (c) 2014-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <validation.h>
#include <net.h>

#include <test/test_unite.h>

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(main_tests, TestingSetup)

static void TestBlockSubsidyHalvings(const Consensus::Params& consensusParams)
{
    int maxHalvings = 64;
    CAmount nInitialSubsidy = 50 * UNIT;

    // UNIT-E TODO PoS reward test should replace this test
}

static void TestBlockSubsidyHalvings(int nSubsidyHalvingInterval)
{
  // UNIT-E TODO PoS reward test should replace this test
}

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    TestBlockSubsidyHalvings(chainParams->GetConsensus()); // As in main
    TestBlockSubsidyHalvings(150); // As in regtest
    TestBlockSubsidyHalvings(1000); // Just another interval
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
//    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
//    CAmount nSum = 0;
//    for (int nHeight = 0; nHeight < 14000000; nHeight += 1000) {
//        CAmount nSubsidy = GetBlockSubsidy(nHeight, chainParams->GetConsensus());
//        BOOST_CHECK(nSubsidy <= 50 * UNIT);
//        nSum += nSubsidy * 1000;
//        BOOST_CHECK(MoneyRange(nSum));
//    }
//    BOOST_CHECK_EQUAL(nSum, 2099999997690000ULL);
  // UNIT-E TODO this needs replacement for PoS limit
}

bool ReturnFalse() { return false; }
bool ReturnTrue() { return true; }

BOOST_AUTO_TEST_CASE(test_combiner_all)
{
    boost::signals2::signal<bool (), CombinerAll> Test;
    BOOST_CHECK(Test());
    Test.connect(&ReturnFalse);
    BOOST_CHECK(!Test());
    Test.connect(&ReturnTrue);
    BOOST_CHECK(!Test());
    Test.disconnect(&ReturnFalse);
    BOOST_CHECK(Test());
    Test.disconnect(&ReturnTrue);
    BOOST_CHECK(Test());
}
BOOST_AUTO_TEST_SUITE_END()
