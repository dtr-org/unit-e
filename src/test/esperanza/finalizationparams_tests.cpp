// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/finalizationparams.h>
#include <test/test_unite.h>
#include <univalue.h>
#include <util.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

BOOST_FIXTURE_TEST_SUITE(finalization_params_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(parse_params_invalid_json) {
  std::string json = R"(
        this is not json {[]}
    )";

  esperanza::FinalizationParams result;
  BOOST_CHECK_EQUAL(esperanza::ParseFinalizationParams(json, result), false);
}

BOOST_AUTO_TEST_CASE(parse_params_param_not_a_number_fallback_default) {
  std::string json = R"(
        {
            "epochLength" : "NotANumber"
        }
    )";

  esperanza::FinalizationParams result;
  esperanza::FinalizationParams defaultParams;
  BOOST_CHECK(esperanza::ParseFinalizationParams(json, result));
  BOOST_CHECK_EQUAL(result.epoch_length, defaultParams.epoch_length);
}

BOOST_AUTO_TEST_CASE(parse_params_negative_unsigned_params) {
  std::string json = R"(
        {
            "baseInterestFactor" : -1
        }
    )";

  esperanza::FinalizationParams result;
  BOOST_CHECK_EQUAL(esperanza::ParseFinalizationParams(json, result), false);

  json = R"(
        {
            "basePenaltyFactor" : -1
        }
    )";
  BOOST_CHECK_EQUAL(esperanza::ParseFinalizationParams(json, result), false);
}

BOOST_AUTO_TEST_CASE(parse_params_values) {
  int64_t epochLength = 10;
  CAmount minDepositSize = 500;
  int64_t withdrawalEpochDelay = 10;

  int64_t bountyFractionDenominator = 2;
  ufp64::ufp64_t baseInterestFactor = ufp64::to_ufp64(7);

  std::string json = R"(
        {
            "epochLength" : 10,
            "minDepositSize": 500,
            "withdrawalEpochDelay" : 10,
            "bountyFractionDenominator" : 2,
            "baseInterestFactor": 7
        }
    )";

  esperanza::FinalizationParams result;
  esperanza::FinalizationParams defaultParams;
  BOOST_CHECK(esperanza::ParseFinalizationParams(json, result));
  BOOST_CHECK_EQUAL(result.epoch_length, epochLength);
  BOOST_CHECK_EQUAL(result.min_deposit_size, minDepositSize);
  BOOST_CHECK_EQUAL(result.dynasty_logout_delay,
                    defaultParams.dynasty_logout_delay);
  BOOST_CHECK_EQUAL(result.withdrawal_epoch_delay, withdrawalEpochDelay);
  BOOST_CHECK_EQUAL(result.slash_fraction_multiplier,
                    defaultParams.slash_fraction_multiplier);
  BOOST_CHECK_EQUAL(result.bounty_fraction_denominator,
                    bountyFractionDenominator);
  BOOST_CHECK_EQUAL(result.base_interest_factor, baseInterestFactor);
  BOOST_CHECK_EQUAL(result.base_penalty_factor,
                    defaultParams.base_penalty_factor);
}

BOOST_AUTO_TEST_SUITE_END()
