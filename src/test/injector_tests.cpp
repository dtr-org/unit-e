// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <injector.h>

#include <blockchain/blockchain_behavior.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(injector_tests)

BOOST_AUTO_TEST_CASE(check_order) {
  UnitEInjector injector;
  BOOST_CHECK_NO_THROW(injector.DetermineInitializationOrder());
}

BOOST_AUTO_TEST_CASE(try_initialize_mainnet) {
  UnitEInjector injector;
  ArgsManager args;
  const char *const argv[] = {
      "./united"};
  args.ParseParameters(1, argv);
  blockchain::Behavior::MakeGlobal(&args);
  BOOST_CHECK_NO_THROW(injector.Initialize());
}

BOOST_AUTO_TEST_CASE(try_initialize_testnet) {
  UnitEInjector injector;
  ArgsManager args;
  const char *const argv[] = {
      "./united",
      "-testnet"};
  args.ParseParameters(2, argv);
  blockchain::Behavior::MakeGlobal(&args);
  BOOST_CHECK_NO_THROW(injector.Initialize());
}

BOOST_AUTO_TEST_CASE(try_initialize_regtest) {
  UnitEInjector injector;
  ArgsManager args;
  const char *const argv[] = {
      "./united",
      "-regtest"};
  args.ParseParameters(2, argv);
  blockchain::Behavior::MakeGlobal(&args);
  BOOST_CHECK_NO_THROW(injector.Initialize());
}

BOOST_AUTO_TEST_SUITE_END()
