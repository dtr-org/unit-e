// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <settings.h>

#include <blockchain/blockchain_behavior.h>

#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(settings_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(pick_settings_test) {

  std::vector<CAmount> values = { 0, 500, 1000 };

  for (const auto v : values) {
    ArgsManager args_manager;
    blockchain::Parameters blockchain_parameters;
    blockchain_parameters.default_settings.stake_combine_maximum = v;
    std::unique_ptr<blockchain::Behavior> blockchain_behavior =
        blockchain::Behavior::NewFromParameters(blockchain_parameters);

    std::unique_ptr<Settings> settings = Settings::New(&args_manager, blockchain_behavior.get());

    BOOST_CHECK_EQUAL(settings->stake_combine_maximum, v);
  }
}

BOOST_AUTO_TEST_CASE(proposer_is_disabled_in_regtest) {

  blockchain::Parameters params = blockchain::Parameters::RegTest();
  BOOST_CHECK(!params.default_settings.node_is_proposer);
}

BOOST_AUTO_TEST_SUITE_END()
