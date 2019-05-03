// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/ismine.h>

#include <key.h>
#include <keystore.h>
#include <outputtype.h>
#include <pubkey.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

#include <array>

BOOST_FIXTURE_TEST_SUITE(ismine_tests, ReducedTestingSetup)

bool IsSpendable(isminetype is_mine) {
  return (is_mine & isminetype::ISMINE_SPENDABLE) != 0;
}

bool IsWatchOnly(isminetype is_mine) {
  return (is_mine & isminetype::ISMINE_WATCH_ONLY) != 0;
}

BOOST_AUTO_TEST_CASE(is_stakeable_by_me_p2wpkh) {

  CBasicKeyStore keystore;

  CKey key;
  key.MakeNewKey(true);
  keystore.AddKey(key);

  const CTxDestination destination = WitnessV0KeyHash(key.GetPubKey().GetID());
  const CScript p2wpkh = GetScriptForDestination(destination);

  BOOST_CHECK(IsSpendable(IsMine(keystore, p2wpkh)));
  BOOST_CHECK(IsStakeableByMe(keystore, p2wpkh));
}

BOOST_AUTO_TEST_CASE(is_not_stakeable_by_me_p2wpkh) {

  CBasicKeyStore keystore;

  CKey key;
  key.MakeNewKey(true);
  // do not add key to the keystore

  const CTxDestination destination = WitnessV0KeyHash(key.GetPubKey().GetID());
  const CScript p2wpkh = GetScriptForDestination(destination);

  BOOST_CHECK(!IsSpendable(IsMine(keystore, p2wpkh)));
  BOOST_CHECK(!IsStakeableByMe(keystore, p2wpkh));
}

BOOST_AUTO_TEST_CASE(is_stakeable_by_me_p2wsh_pubkey) {

  CBasicKeyStore keystore;

  CKey key;
  key.MakeNewKey(true);
  BOOST_CHECK(keystore.AddKey(key));

  const CScript script = GetScriptForRawPubKey(key.GetPubKey());
  uint256 script_hash;
  CSHA256().Write(&script[0], script.size()).Finalize(script_hash.begin());

  keystore.AddCScript(script);

  const CTxDestination destination = WitnessV0ScriptHash(script_hash);
  const CScript p2wsh_script = GetScriptForDestination(destination);

  keystore.AddCScript(p2wsh_script);

  BOOST_CHECK(IsSpendable(IsMine(keystore, p2wsh_script)));
  BOOST_CHECK(IsStakeableByMe(keystore, p2wsh_script));
}

BOOST_AUTO_TEST_CASE(is_not_stakeable_by_me_p2wsh_pubkey_watchonly) {

  CBasicKeyStore keystore;

  CKey key;
  key.MakeNewKey(true);
  // do not add key to the keystore

  const CScript script = GetScriptForRawPubKey(key.GetPubKey());
  uint256 script_hash;
  CSHA256().Write(&script[0], script.size()).Finalize(script_hash.begin());

  BOOST_CHECK(keystore.AddWatchOnly(script));

  const CTxDestination destination = WitnessV0ScriptHash(script_hash);
  const CScript p2wsh_script = GetScriptForDestination(destination);

  BOOST_CHECK(keystore.AddWatchOnly(p2wsh_script));

  const isminetype is_mine = IsMine(keystore, p2wsh_script);
  BOOST_CHECK(!IsSpendable(is_mine));
  BOOST_CHECK(IsWatchOnly(is_mine));
  BOOST_CHECK(!IsStakeableByMe(keystore, p2wsh_script));
}

BOOST_AUTO_TEST_CASE(is_not_stakeable_by_me_p2wsh_pubkey_unknown) {

  CBasicKeyStore keystore;

  CKey key;
  key.MakeNewKey(true);
  // do not add key to the keystore

  const CScript script = GetScriptForRawPubKey(key.GetPubKey());
  uint256 script_hash;
  CSHA256().Write(&script[0], script.size()).Finalize(script_hash.begin());

  const CTxDestination destination = WitnessV0ScriptHash(script_hash);
  const CScript p2wsh_script = GetScriptForDestination(destination);

  const isminetype is_mine = IsMine(keystore, p2wsh_script);
  BOOST_CHECK(!IsSpendable(is_mine));
  BOOST_CHECK(!IsWatchOnly(is_mine));
  BOOST_CHECK(!IsStakeableByMe(keystore, p2wsh_script));
}

BOOST_AUTO_TEST_CASE(is_stakeable_by_me_p2wsh_pubkeyhash) {

  CBasicKeyStore keystore;

  CKey key;
  key.MakeNewKey(true);
  BOOST_CHECK(keystore.AddKey(key));

  const CScript script = GetScriptForDestination(key.GetPubKey().GetID());
  uint256 script_hash;
  CSHA256().Write(&script[0], script.size()).Finalize(script_hash.begin());

  BOOST_CHECK(keystore.AddCScript(script));

  const CTxDestination destination = WitnessV0ScriptHash(script_hash);
  const CScript p2wsh_script = GetScriptForDestination(destination);

  BOOST_CHECK(keystore.AddCScript(p2wsh_script));

  BOOST_CHECK(IsSpendable(IsMine(keystore, p2wsh_script)));
  BOOST_CHECK(IsStakeableByMe(keystore, p2wsh_script));
}

BOOST_AUTO_TEST_CASE(is_not_stakeable_by_me_p2wsh_pubkeyhash_watchonly) {

  CBasicKeyStore keystore;

  CKey key;
  key.MakeNewKey(true);
  // do not add key to the keystore

  const CScript script = GetScriptForDestination(key.GetPubKey().GetID());
  uint256 script_hash;
  CSHA256().Write(&script[0], script.size()).Finalize(script_hash.begin());

  BOOST_CHECK(keystore.AddWatchOnly(script));

  const CTxDestination destination = WitnessV0ScriptHash(script_hash);
  const CScript p2wsh_script = GetScriptForDestination(destination);

  BOOST_CHECK(keystore.AddWatchOnly(p2wsh_script));

  const isminetype is_mine = IsMine(keystore, p2wsh_script);
  BOOST_CHECK(!IsSpendable(is_mine));
  BOOST_CHECK(IsWatchOnly(is_mine));
  BOOST_CHECK(!IsStakeableByMe(keystore, p2wsh_script));
}

BOOST_AUTO_TEST_CASE(is_not_stakeable_by_me_p2wsh_pubkeyhash_unknown) {

  CBasicKeyStore keystore;

  CKey key;
  key.MakeNewKey(true);
  // do not add key to the keystore

  const CScript script = GetScriptForDestination(key.GetPubKey().GetID());
  uint256 script_hash;
  CSHA256().Write(&script[0], script.size()).Finalize(script_hash.begin());

  const CTxDestination destination = WitnessV0ScriptHash(script_hash);
  const CScript p2wsh_script = GetScriptForDestination(destination);

  const isminetype is_mine = IsMine(keystore, p2wsh_script);
  BOOST_CHECK(!IsSpendable(is_mine));
  BOOST_CHECK(!IsWatchOnly(is_mine));
  BOOST_CHECK(!IsStakeableByMe(keystore, p2wsh_script));
}

BOOST_AUTO_TEST_CASE(is_stakeable_by_me_remote_staking_watchonly) {
  CBasicKeyStore keystore;

  CKey key;
  key.MakeNewKey(true);

  const CScript script = GetScriptForRawPubKey(key.GetPubKey());
  // keystore.AddWatchOnly adds not only the script but also the public key
  BOOST_CHECK(keystore.AddWatchOnly(script));

  const auto staking_key_hash = ToByteVector(key.GetPubKey().GetID());
  const std::vector<unsigned char> dummy_hash(32, 1);
  const CScript rsp2wpkh = CScript::CreateRemoteStakingKeyhashScript(staking_key_hash, dummy_hash);

  BOOST_CHECK(!IsSpendable(IsMine(keystore, rsp2wpkh)));
  // keystore has only the staking public key but not the private key
  BOOST_CHECK(!IsStakeableByMe(keystore, rsp2wpkh));

  const CScript rsp2wsh = CScript::CreateRemoteStakingScripthashScript(staking_key_hash, dummy_hash);

  BOOST_CHECK(!IsSpendable(IsMine(keystore, rsp2wsh)));
  BOOST_CHECK(!IsStakeableByMe(keystore, rsp2wsh));
}

BOOST_AUTO_TEST_CASE(is_stakeable_by_me_destinations) {

  // Test that the P2PSH output type is NOT stakeable
  {
    CKey key;
    key.MakeNewKey(true);
    CBasicKeyStore keystore;
    keystore.AddKey(key);

    CTxDestination dest = GetDestinationForKey(key.GetPubKey(), OutputType::P2SH_SEGWIT);
    CScript script = GetScriptForDestination(dest);

    BOOST_CHECK(!IsStakeableByMe(keystore, script));
  }

  // Test that the legacy output type is stakeable
  {
    CKey key;
    key.MakeNewKey(true);
    CBasicKeyStore keystore;
    keystore.AddKey(key);

    CTxDestination dest = GetDestinationForKey(key.GetPubKey(), OutputType::LEGACY);
    CScript script = GetScriptForDestination(dest);

    BOOST_CHECK(IsStakeableByMe(keystore, script));
  }

  // Test that the bech32 output type is stakeable
  {
    CKey key;
    key.MakeNewKey(true);
    CBasicKeyStore keystore;
    keystore.AddKey(key);

    CTxDestination dest = GetDestinationForKey(key.GetPubKey(), OutputType::BECH32);
    CScript script = GetScriptForDestination(dest);

    BOOST_CHECK(IsStakeableByMe(keystore, script));
  }
}

BOOST_AUTO_TEST_SUITE_END()
