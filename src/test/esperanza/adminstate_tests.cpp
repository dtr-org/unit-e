// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <addrman.h>
#include <esperanza/admincommand.h>
#include <esperanza/adminstate.h>
#include <test/esperanza/finalizationstate_utils.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(adminstate_tests, ReducedTestingSetup)

CPubKey MakePubKey() {
  CKey key;
  key.MakeNewKey(true);
  return key.GetPubKey();
}

esperanza::AdminKeySet MakeKeySet() {
  return {{MakePubKey(), MakePubKey(), MakePubKey()}};
}

BOOST_AUTO_TEST_CASE(empty_params_mean_no_admin) {
  esperanza::AdminParams emptyParams;
  esperanza::AdminState state(emptyParams);

  const uint160 validatorAddress = RandValidatorAddr();

  BOOST_CHECK(state.IsValidatorAuthorized(validatorAddress));
}

BOOST_AUTO_TEST_CASE(reset_admin) {
  const esperanza::AdminKeySet set0 = MakeKeySet();
  const esperanza::AdminKeySet set1 = MakeKeySet();

  esperanza::AdminParams params;
  params.admin_keys = set0;

  esperanza::AdminState state(params);

  BOOST_CHECK(state.IsAdminAuthorized(set0));
  BOOST_CHECK(!state.IsAdminAuthorized(set1));

  state.ResetAdmin(set1);

  BOOST_CHECK(!state.IsAdminAuthorized(set0));
  BOOST_CHECK(state.IsAdminAuthorized(set1));
}

BOOST_AUTO_TEST_CASE(change_white_list) {
  esperanza::AdminParams params;
  params.admin_keys = MakeKeySet();
  esperanza::AdminState state(params);

  const uint160 validator = RandValidatorAddr();

  BOOST_CHECK(!state.IsValidatorAuthorized(validator));

  state.AddValidator(validator);

  BOOST_CHECK(state.IsValidatorAuthorized(validator));

  state.RemoveValidator(validator);

  BOOST_CHECK(!state.IsValidatorAuthorized(validator));
}

BOOST_AUTO_TEST_CASE(end_permissioning) {
  esperanza::AdminParams params;
  params.admin_keys = MakeKeySet();
  esperanza::AdminState state(params);

  const uint160 validator = RandValidatorAddr();

  BOOST_CHECK(!state.IsValidatorAuthorized(validator));

  state.EndPermissioning();

  BOOST_CHECK(state.IsValidatorAuthorized(validator));
}

BOOST_AUTO_TEST_SUITE_END()
