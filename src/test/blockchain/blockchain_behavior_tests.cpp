// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/wallet.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>
#include <blockchain/blockchain_parameters.h>

namespace {
  struct Fixture {

    ArgsManager args_manager;

    std::unique_ptr<blockchain::Behavior> blockchain_behavior =
        blockchain::Behavior::New(&args_manager);

    CWallet wallet;

    staking::StakingWallet &staking_wallet = wallet.GetWalletExtension();

    CBlock block;

    void MkBlock() {



    }

  };
}

BOOST_FIXTURE_TEST_SUITE(blockchain_behavior_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(extract_block_signing_key) {

  Fixture fixture;

}

BOOST_AUTO_TEST_SUITE_END()
