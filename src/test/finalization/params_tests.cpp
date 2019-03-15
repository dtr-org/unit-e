// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/finalizationparams.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(finalization_params_tests)

BOOST_AUTO_TEST_CASE(get_epoch_start_height) {
  esperanza::FinalizationParams params;
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
  esperanza::FinalizationParams params;
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

BOOST_AUTO_TEST_SUITE_END()
