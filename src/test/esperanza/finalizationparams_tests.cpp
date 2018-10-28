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
  BOOST_CHECK_EQUAL(result.m_epochLength, defaultParams.m_epochLength);
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
  BOOST_CHECK_EQUAL(result.m_epochLength, epochLength);
  BOOST_CHECK_EQUAL(result.m_minDepositSize, minDepositSize);
  BOOST_CHECK_EQUAL(result.m_dynastyLogoutDelay,
                    defaultParams.m_dynastyLogoutDelay);
  BOOST_CHECK_EQUAL(result.m_withdrawalEpochDelay, withdrawalEpochDelay);
  BOOST_CHECK_EQUAL(result.m_slashFractionMultiplier,
                    defaultParams.m_slashFractionMultiplier);
  BOOST_CHECK_EQUAL(result.m_bountyFractionDenominator,
                    bountyFractionDenominator);
  BOOST_CHECK_EQUAL(result.m_baseInterestFactor, baseInterestFactor);
  BOOST_CHECK_EQUAL(result.m_basePenaltyFactor,
                    defaultParams.m_basePenaltyFactor);
}

BOOST_AUTO_TEST_SUITE_END()
