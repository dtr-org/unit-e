// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/params.h>
#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(finalization_params_tests)

BOOST_AUTO_TEST_CASE(get_epoch) {
  std::map<uint32_t, uint32_t> height_to_epoch{
      {0, 0},
      {1, 1},
      {2, 1},
      {3, 1},
      {4, 1},
      {5, 1},
      {6, 2},
      {9, 2},
      {10, 2},
      {11, 3},
      {15, 3},
      {16, 4},
      {20, 4},
      {25, 5},
  };

  finalization::Params params;
  params.epoch_length = 5;

  for (const auto &it : height_to_epoch) {
    BOOST_CHECK_EQUAL(params.GetEpoch(it.first), it.second);
  }
}

BOOST_AUTO_TEST_CASE(get_epoch_start_height) {
  finalization::Params params;
  params.epoch_length = 5;

  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(0), 0);
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(1), 1);
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(2), 6);
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(3), 11);

  params.epoch_length = 42;
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(0), 0);
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(1), 1);
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(2), 43);
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(3), 85);
}

BOOST_AUTO_TEST_CASE(get_epoch_checkpoint_height) {
  finalization::Params params;
  params.epoch_length = 5;

  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(0), 0);
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(1), 5);
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(2), 10);
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(3), 15);

  params.epoch_length = 50;
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(0), 0);
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(1), 50);
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(2), 100);
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(3), 150);
}

BOOST_AUTO_TEST_CASE(is_epoch_start) {
  finalization::Params params;
  params.epoch_length = 5;

  BOOST_CHECK(!params.IsEpochStart(0));
  BOOST_CHECK(params.IsEpochStart(1));
  BOOST_CHECK(!params.IsEpochStart(2));
  BOOST_CHECK(!params.IsEpochStart(3));
  BOOST_CHECK(!params.IsEpochStart(4));
  BOOST_CHECK(!params.IsEpochStart(5));
  BOOST_CHECK(params.IsEpochStart(6));
  BOOST_CHECK(params.IsEpochStart(11));

  params.epoch_length = 42;
  BOOST_CHECK(!params.IsEpochStart(0));
  BOOST_CHECK(params.IsEpochStart(1));
  BOOST_CHECK(!params.IsEpochStart(2));
  BOOST_CHECK(!params.IsEpochStart(6));
  BOOST_CHECK(params.IsEpochStart(43));
  BOOST_CHECK(params.IsEpochStart(85));
}

BOOST_AUTO_TEST_CASE(is_checkpoint) {
  finalization::Params params;
  params.epoch_length = 5;

  BOOST_CHECK(params.IsCheckpoint(0));
  BOOST_CHECK(!params.IsCheckpoint(1));
  BOOST_CHECK(!params.IsCheckpoint(2));
  BOOST_CHECK(!params.IsCheckpoint(3));
  BOOST_CHECK(!params.IsCheckpoint(4));
  BOOST_CHECK(params.IsCheckpoint(5));
  BOOST_CHECK(!params.IsCheckpoint(6));
  BOOST_CHECK(params.IsCheckpoint(10));

  params.epoch_length = 11;
  BOOST_CHECK(params.IsCheckpoint(0));
  BOOST_CHECK(!params.IsCheckpoint(1));
  BOOST_CHECK(!params.IsCheckpoint(2));
  BOOST_CHECK(!params.IsCheckpoint(5));
  BOOST_CHECK(params.IsCheckpoint(11));
  BOOST_CHECK(params.IsCheckpoint(22));
}

BOOST_AUTO_TEST_CASE(parse_params_invalid_json) {
  const std::string json = R"(
        this is not json {[]}
    )";

  UnitEInjectorConfiguration config;
  mocks::ArgsManagerMock args{"-esperanzaconfig=" + json};

  BOOST_CHECK_THROW(finalization::Params::New(&config, &args), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(parse_params_param_not_a_number_fallback_default) {
  const std::string json = R"(
        {
            "epochLength" : "NotANumber"
        }
    )";

  UnitEInjectorConfiguration config;
  mocks::ArgsManagerMock args{"-esperanzaconfig=" + json, "-regtest"};
  const auto result = finalization::Params::New(&config, &args);
  const auto default_params = finalization::Params::RegTest();
  BOOST_CHECK_EQUAL(result->epoch_length, default_params.epoch_length);
}

BOOST_AUTO_TEST_CASE(parse_params_negative_unsigned_params) {
  UnitEInjectorConfiguration config;
  {
    const std::string json = R"(
        {
            "baseInterestFactor" : -1
        }
    )";

    mocks::ArgsManagerMock args{"-esperanzaconfig=" + json, "-regtest"};
    BOOST_CHECK_THROW(finalization::Params::New(&config, &args), std::runtime_error);
  }

  {
    const std::string json = R"(
        {
            "basePenaltyFactor" : -1
        }
    )";

    mocks::ArgsManagerMock args{"-esperanzaconfig=" + json, "-regtest"};
    BOOST_CHECK_THROW(finalization::Params::New(&config, &args), std::runtime_error);
  }
}

BOOST_AUTO_TEST_CASE(parse_params_values) {
  const std::string json = R"(
        {
            "epochLength" : 10,
            "minDepositSize": 500,
            "withdrawalEpochDelay" : 10,
            "bountyFractionDenominator" : 2,
            "baseInterestFactor": 700000000
        }
    )";

  UnitEInjectorConfiguration config;
  mocks::ArgsManagerMock args{"-esperanzaconfig=" + json, "-regtest"};
  const auto result = finalization::Params::New(&config, &args);
  const auto default_params = finalization::Params::RegTest();

  BOOST_CHECK_EQUAL(result->epoch_length, 10);
  BOOST_CHECK_EQUAL(result->min_deposit_size, 500);
  BOOST_CHECK_EQUAL(result->dynasty_logout_delay,
                    default_params.dynasty_logout_delay);
  BOOST_CHECK_EQUAL(result->withdrawal_epoch_delay, 10);
  BOOST_CHECK_EQUAL(result->slash_fraction_multiplier,
                    default_params.slash_fraction_multiplier);
  BOOST_CHECK_EQUAL(result->bounty_fraction_denominator, 2);

  BOOST_CHECK_EQUAL(result->base_interest_factor, ufp64::to_ufp64(7));
  BOOST_CHECK_EQUAL(result->base_penalty_factor,
                    default_params.base_penalty_factor);
}

BOOST_AUTO_TEST_CASE(permissioning) {
  UnitEInjectorConfiguration config;
  {
    mocks::ArgsManagerMock args{"-regtest"};
    const auto result = finalization::Params::New(&config, &args);
    BOOST_CHECK_EQUAL(static_cast<bool>(result->admin_params.admin_keys), false);
  }
  {
    mocks::ArgsManagerMock args{"-regtest", "-permissioning=0"};
    const auto result = finalization::Params::New(&config, &args);
    BOOST_CHECK_EQUAL(static_cast<bool>(result->admin_params.admin_keys), false);
  }
  {
    mocks::ArgsManagerMock args{"-regtest", "-permissioning=1"};
    const auto result = finalization::Params::New(&config, &args);
    BOOST_CHECK_EQUAL(static_cast<bool>(result->admin_params.admin_keys), true);
  }
  {
    mocks::ArgsManagerMock args{"-testnet"};
    const auto result = finalization::Params::New(&config, &args);
    BOOST_CHECK_EQUAL(static_cast<bool>(result->admin_params.admin_keys), true);
  }
  {
    mocks::ArgsManagerMock args{"-testnet", "-permissioning=0"};
    const auto result = finalization::Params::New(&config, &args);
    BOOST_CHECK_EQUAL(static_cast<bool>(result->admin_params.admin_keys), true);
  }
  {
    mocks::ArgsManagerMock args{"-testnet", "-permissioning=1"};
    const auto result = finalization::Params::New(&config, &args);
    BOOST_CHECK_EQUAL(static_cast<bool>(result->admin_params.admin_keys), true);
  }
}

BOOST_AUTO_TEST_SUITE_END()
