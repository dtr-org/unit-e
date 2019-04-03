// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <settings.h>

#include <base58.h>
#include <blockchain/blockchain_behavior.h>

#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(settings_tests)

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

class StakeReturnModeVisitor : public boost::static_visitor<std::string> {
 public:
  std::string operator()(staking::ReturnStakeToSameAddress) const {
    return "same";
  }

  std::string operator()(staking::ReturnStakeToNewAddress) const {
    return "new";
  }

  std::string operator()(const CScript &target_script) const {
    return EncodeBase16(ToByteVector(target_script));
  }
};

BOOST_AUTO_TEST_CASE(stake_return_address_test) {
  blockchain::Parameters blockchain_parameters = blockchain::Parameters::RegTest();
  std::unique_ptr<blockchain::Behavior> blockchain_behavior =
      blockchain::Behavior::NewFromParameters(blockchain_parameters);
  std::string stake_return_address = "uert1qxktc85fwgqswkaswtkkqmjsyx0s8xmshekplmh";
  mocks::ArgsManagerMock args_manager{
    "-stakereturnaddress=" + stake_return_address
  };
  std::unique_ptr<Settings> settings = Settings::New(&args_manager, blockchain_behavior.get());

  const CTxDestination destination = DecodeDestination(stake_return_address, *blockchain_behavior);
  const CScript target_script = GetScriptForDestination(destination);

  const std::string result = boost::apply_visitor(StakeReturnModeVisitor(), settings->stake_return_mode);
  const std::string expected = EncodeBase16(ToByteVector(target_script));

  BOOST_CHECK_EQUAL(result, expected);
}

BOOST_AUTO_TEST_SUITE_END()
