// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <injector.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(injector_tests)

BOOST_AUTO_TEST_CASE(check_order) {
  UnitEInjector injector;
  BOOST_CHECK_NO_THROW(injector.DetermineInitializationOrder());
}

BOOST_AUTO_TEST_SUITE_END()
