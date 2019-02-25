// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_custom_parameters.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

using namespace blockchain;

BOOST_AUTO_TEST_SUITE(blockchain_custom_parameters_tests)

BOOST_AUTO_TEST_CASE(load_all_from_json) {

  const boost::optional<blockchain::Parameters> custom_parameters = blockchain::ReadCustomParametersFromJsonString(
      "{"
      "\"network_name\":\"fantasyland\","
      "\"block_stake_timestamp_interval_seconds\":4710,"
      "\"block_time_seconds\":4711,"
      "\"max_future_block_time_seconds\":4712,"
      "\"relay_non_standard_transactions\":false,"
      "\"maximum_block_size\":4713,"
      "\"maximum_block_weight\":4714,"
      "\"maximum_block_serialized_size\":4715,"
      "\"coinbase_maturity\":4716,"
      "\"stake_maturity\":4717,"
      "\"initial_supply\":4718,"
      "\"maximum_supply\":4719,"
      "\"reward_schedule\":[9,8,7,6],"
      "\"period_blocks\":4720,"
      "\"mine_blocks_on_demand\":false,"
      "\"bech32_human_readable_prefix\":\"pfx\","
      "\"deployment_confirmation_period\":4721,"
      "\"rule_change_activation_threshold\":4722,"
      "\"unknown_keys_are_ignored\":true}",
      blockchain::Parameters::RegTest());

  BOOST_REQUIRE(static_cast<bool>(custom_parameters));

  BOOST_CHECK_EQUAL(custom_parameters->network_name, "fantasyland");
  BOOST_CHECK_EQUAL(custom_parameters->block_stake_timestamp_interval_seconds, 4710);
  BOOST_CHECK_EQUAL(custom_parameters->block_time_seconds, 4711);
  BOOST_CHECK_EQUAL(custom_parameters->max_future_block_time_seconds, 4712);
  BOOST_CHECK_EQUAL(custom_parameters->relay_non_standard_transactions, false);
  BOOST_CHECK_EQUAL(custom_parameters->maximum_block_size, 4713);
  BOOST_CHECK_EQUAL(custom_parameters->maximum_block_weight, 4714);
  BOOST_CHECK_EQUAL(custom_parameters->maximum_block_serialized_size, 4715);
  BOOST_CHECK_EQUAL(custom_parameters->coinbase_maturity, 4716);
  BOOST_CHECK_EQUAL(custom_parameters->stake_maturity, 4717);
  BOOST_CHECK_EQUAL(custom_parameters->initial_supply, 4718);
  BOOST_CHECK_EQUAL(custom_parameters->maximum_supply, 4719);
  const std::vector<std::int64_t> expected_reward_schedule = {9, 8, 7, 6};
  BOOST_CHECK_EQUAL(custom_parameters->reward_schedule, expected_reward_schedule);
  BOOST_CHECK_EQUAL(custom_parameters->period_blocks, 4720);
  BOOST_CHECK_EQUAL(custom_parameters->mine_blocks_on_demand, false);
  BOOST_CHECK_EQUAL(custom_parameters->bech32_human_readable_prefix, "pfx");
  BOOST_CHECK_EQUAL(custom_parameters->deployment_confirmation_period, 4721);
  BOOST_CHECK_EQUAL(custom_parameters->rule_change_activation_threshold, 4722);
}

BOOST_AUTO_TEST_CASE(fallback_to_base_parameters) {

  const blockchain::Parameters &fallback_parameters = blockchain::Parameters::RegTest();
  const boost::optional<blockchain::Parameters> custom_parameters = blockchain::ReadCustomParametersFromJsonString(
      "{"
      "\"network_name\":\"fantasyland\","
      "\"block_stake_timestamp_interval_seconds\":4710,"
      "\"relay_non_standard_transactions\":true,"
      "\"maximum_block_size\":4713,"
      "\"maximum_block_weight\":4714,"
      "\"maximum_block_serialized_size\":4715,"
      "\"coinbase_maturity\":4716,"
      "\"period_blocks\":4720,"
      "\"mine_blocks_on_demand\":true,"
      "\"deployment_confirmation_period\":4721,"
      "\"rule_change_activation_threshold\":4722,"
      "\"unknown_keys_are_ignored\":true}",
      fallback_parameters);

  BOOST_REQUIRE(static_cast<bool>(custom_parameters));

  BOOST_CHECK_EQUAL(custom_parameters->network_name, "fantasyland");
  BOOST_CHECK_EQUAL(custom_parameters->block_stake_timestamp_interval_seconds, 4710);
  BOOST_CHECK_EQUAL(custom_parameters->block_time_seconds, fallback_parameters.block_time_seconds);
  BOOST_CHECK_EQUAL(custom_parameters->max_future_block_time_seconds, fallback_parameters.max_future_block_time_seconds);
  BOOST_CHECK_EQUAL(custom_parameters->relay_non_standard_transactions, true);
  BOOST_CHECK_EQUAL(custom_parameters->maximum_block_size, 4713);
  BOOST_CHECK_EQUAL(custom_parameters->maximum_block_weight, 4714);
  BOOST_CHECK_EQUAL(custom_parameters->maximum_block_serialized_size, 4715);
  BOOST_CHECK_EQUAL(custom_parameters->coinbase_maturity, 4716);
  BOOST_CHECK_EQUAL(custom_parameters->stake_maturity, fallback_parameters.stake_maturity);
  BOOST_CHECK_EQUAL(custom_parameters->initial_supply, fallback_parameters.initial_supply);
  BOOST_CHECK_EQUAL(custom_parameters->maximum_supply, fallback_parameters.maximum_supply);
  BOOST_CHECK_EQUAL(custom_parameters->reward_schedule, fallback_parameters.reward_schedule);
  BOOST_CHECK_EQUAL(custom_parameters->period_blocks, 4720);
  BOOST_CHECK_EQUAL(custom_parameters->mine_blocks_on_demand, true);
  BOOST_CHECK_EQUAL(custom_parameters->bech32_human_readable_prefix, fallback_parameters.bech32_human_readable_prefix);
  BOOST_CHECK_EQUAL(custom_parameters->deployment_confirmation_period, 4721);
  BOOST_CHECK_EQUAL(custom_parameters->rule_change_activation_threshold, 4722);
}

BOOST_AUTO_TEST_CASE(error_reporting) {

  std::set<std::string> errors;
  const boost::optional<blockchain::Parameters> custom_parameters = blockchain::ReadCustomParametersFromJsonString(
      "{"
      "\"network_name\":true,"
      "\"block_stake_timestamp_interval_seconds\":-4710,"
      "\"block_time_seconds\":47119872349873054,"                          // bigger than std::uint32_t
      "\"maximum_block_size\":47119872349873054239473490232131200271801,"  // bigger than std::int64_t
      "\"max_future_block_time_seconds\":\"i call bull\","
      "\"unknown_keys_are_ignored\":true}",
      blockchain::Parameters::RegTest(),
      [&errors](const std::string &error) { errors.emplace(error); });

  BOOST_CHECK(!static_cast<bool>(custom_parameters));

  std::set<std::string> expected_errors;
  expected_errors.emplace("Failed to read \"network_name\"");
  expected_errors.emplace("Failed to read \"block_stake_timestamp_interval_seconds\"");
  expected_errors.emplace("Failed to read \"block_time_seconds\"");
  expected_errors.emplace("Failed to read \"maximum_block_size\"");
  expected_errors.emplace("Failed to read \"max_future_block_time_seconds\"");
  BOOST_CHECK_EQUAL(errors, expected_errors);
}

BOOST_AUTO_TEST_SUITE_END()