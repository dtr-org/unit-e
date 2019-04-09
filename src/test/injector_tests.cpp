// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <injector.h>

#include <blockchain/blockchain_behavior.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(injector_tests)

static const UnitEInjectorConfiguration config = [](){
  UnitEInjectorConfiguration cfg;
  cfg.use_in_memory_databases = true;
  return cfg;
}();

BOOST_AUTO_TEST_CASE(check_order) {
  UnitEInjector injector(config);
  BOOST_CHECK_NO_THROW(injector.DetermineInitializationOrder());
}

BOOST_AUTO_TEST_CASE(try_initialize_mainnet) {
  UnitEInjector injector(config);
  ArgsManager args;
  std::string error;
  const char *const argv[] = {
      "./unit-e"};
  args.ParseParameters(1, argv, error);
  blockchain::Behavior::MakeGlobal(&args);
  BOOST_CHECK_NO_THROW(injector.Initialize());
}

BOOST_AUTO_TEST_CASE(try_initialize_testnet) {
  UnitEInjector injector(config);
  ArgsManager args;
  std::string error;
  const char *const argv[] = {
      "./unit-e",
      "-testnet"};
  args.ParseParameters(2, argv, error);
  blockchain::Behavior::MakeGlobal(&args);
  BOOST_CHECK_NO_THROW(injector.Initialize());
}

BOOST_AUTO_TEST_CASE(try_initialize_regtest) {
  UnitEInjector injector(config);
  ArgsManager args;
  std::string error;
  const char *const argv[] = {
      "./unit-e",
      "-regtest"};
  args.ParseParameters(2, argv, error);
  blockchain::Behavior::MakeGlobal(&args);
  BOOST_CHECK_NO_THROW(injector.Initialize());
}

BOOST_AUTO_TEST_SUITE_END()
