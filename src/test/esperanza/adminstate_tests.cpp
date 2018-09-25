// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <addrman.h>
#include <esperanza/admincommand.h>
#include <esperanza/adminstate.h>
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
  esperanza::AdminParams emptyParams = {};
  esperanza::AdminState state(emptyParams);

  const auto validatorIndex = MakePubKey().GetHash();

  BOOST_CHECK(state.IsValidatorAuthorized(validatorIndex));
}

BOOST_AUTO_TEST_CASE(reset_admin_soft) {
  const auto set0 = MakeKeySet();
  const auto set1 = MakeKeySet();

  esperanza::AdminParams params = {};
  params.m_blockToAdminKeys.emplace(0, set0);

  esperanza::AdminState state(params);
  state.OnBlock(0);

  BOOST_CHECK(state.IsAdminAuthorized(set0));
  BOOST_CHECK(!state.IsAdminAuthorized(set1));

  state.ResetAdmin(set1);

  BOOST_CHECK(!state.IsAdminAuthorized(set0));
  BOOST_CHECK(state.IsAdminAuthorized(set1));
}

BOOST_AUTO_TEST_CASE(reset_admin_hard) {
  const auto set0 = MakeKeySet();
  const auto set1 = MakeKeySet();

  esperanza::AdminParams params = {};
  params.m_blockToAdminKeys.emplace(0, set0);
  params.m_blockToAdminKeys.emplace(42, set1);

  esperanza::AdminState state(params);
  state.OnBlock(0);

  BOOST_CHECK(state.IsAdminAuthorized(set0));
  BOOST_CHECK(!state.IsAdminAuthorized(set1));

  state.OnBlock(10);

  BOOST_CHECK(state.IsAdminAuthorized(set0));
  BOOST_CHECK(!state.IsAdminAuthorized(set1));

  state.OnBlock(42);

  BOOST_CHECK(!state.IsAdminAuthorized(set0));
  BOOST_CHECK(state.IsAdminAuthorized(set1));

  state.OnBlock(100);

  BOOST_CHECK(!state.IsAdminAuthorized(set0));
  BOOST_CHECK(state.IsAdminAuthorized(set1));
}

BOOST_AUTO_TEST_CASE(change_white_list_soft) {
  esperanza::AdminParams params = {};
  params.m_blockToAdminKeys.emplace(0, MakeKeySet());
  esperanza::AdminState state(params);

  const auto validator = MakePubKey().GetHash();

  BOOST_CHECK(!state.IsValidatorAuthorized(validator));

  state.AddValidator(validator);

  BOOST_CHECK(state.IsValidatorAuthorized(validator));

  state.RemoveValidator(validator);

  BOOST_CHECK(!state.IsValidatorAuthorized(validator));
}

BOOST_AUTO_TEST_CASE(change_white_list_hard) {
  esperanza::AdminParams params = {};
  params.m_blockToAdminKeys.emplace(0, MakeKeySet());

  const auto validator1 = MakePubKey().GetHash();
  const auto validator2 = MakePubKey().GetHash();

  params.m_blockToWhiteList[0] = {};
  params.m_blockToWhiteList[1] = {validator1};
  params.m_blockToWhiteList[2] = {validator1, validator2};
  params.m_blockToWhiteList[3] = {validator2};
  params.m_blockToWhiteList[4] = {};

  esperanza::AdminState state(params);

  BOOST_CHECK(!state.IsValidatorAuthorized(validator1));
  BOOST_CHECK(!state.IsValidatorAuthorized(validator2));

  state.OnBlock(1);

  BOOST_CHECK(state.IsValidatorAuthorized(validator1));
  BOOST_CHECK(!state.IsValidatorAuthorized(validator2));

  state.OnBlock(2);

  BOOST_CHECK(state.IsValidatorAuthorized(validator1));
  BOOST_CHECK(state.IsValidatorAuthorized(validator2));

  state.OnBlock(3);

  BOOST_CHECK(!state.IsValidatorAuthorized(validator1));
  BOOST_CHECK(state.IsValidatorAuthorized(validator2));

  // To check that hard reset is actually reset
  state.AddValidator(validator1);

  BOOST_CHECK(state.IsValidatorAuthorized(validator1));
  BOOST_CHECK(state.IsValidatorAuthorized(validator2));

  state.OnBlock(4);
  BOOST_CHECK(!state.IsValidatorAuthorized(validator1));
  BOOST_CHECK(!state.IsValidatorAuthorized(validator2));
}

BOOST_AUTO_TEST_CASE(end_permissioning) {
  esperanza::AdminParams params = {};
  params.m_blockToAdminKeys.emplace(0, MakeKeySet());
  esperanza::AdminState state(params);

  const auto validator = MakePubKey().GetHash();

  BOOST_CHECK(!state.IsValidatorAuthorized(validator));

  state.EndPermissioning();

  BOOST_CHECK(state.IsValidatorAuthorized(validator));
}

BOOST_AUTO_TEST_SUITE_END()
