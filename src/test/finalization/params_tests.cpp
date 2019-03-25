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
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(1), 5);
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(2), 10);

  params.epoch_length = 42;
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(0), 0);
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(1), 42);
  BOOST_CHECK_EQUAL(params.GetEpochStartHeight(2), 84);
}

BOOST_AUTO_TEST_CASE(get_epoch_checkpoint_height) {
  esperanza::FinalizationParams params;
  params.epoch_length = 5;

  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(0), 4);
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(1), 9);
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(2), 14);

  params.epoch_length = 50;
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(0), 49);
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(1), 99);
  BOOST_CHECK_EQUAL(params.GetEpochCheckpointHeight(2), 149);
}

BOOST_AUTO_TEST_SUITE_END()
