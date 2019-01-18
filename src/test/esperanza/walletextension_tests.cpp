// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <base58.h>
#include <blockchain/blockchain_behavior.h>
#include <consensus/validation.h>
#include <esperanza/finalizationparams.h>
#include <esperanza/vote.h>
#include <key.h>
#include <keystore.h>
#include <primitives/transaction.h>
#include <primitives/txtype.h>
#include <script/script.h>
#include <test/esperanza/finalization_utils.h>
#include <test/test_unite.h>
#include <txmempool.h>
#include <validation.h>
#include <base58.cpp>
#include <boost/test/unit_test.hpp>
#include <chainparams.cpp>

BOOST_AUTO_TEST_SUITE(walletextension_tests)

BOOST_FIXTURE_TEST_CASE(vote_signature, ReducedTestingSetup) {

  CBasicKeyStore keystore;
  CKey k;
  InsecureNewKey(k, true);
  keystore.AddKey(k);

  CPubKey pk = k.GetPubKey();

  esperanza::Vote vote{pk.GetID(), GetRandHash(), 10, 100};
  std::vector<unsigned char> voteSig;
  BOOST_CHECK(esperanza::Vote::CreateSignature(&keystore, vote, voteSig));

  std::vector<unsigned char> expectedSig;
  k.Sign(vote.GetHash(), expectedSig);

  BOOST_CHECK_EQUAL(HexStr(expectedSig), HexStr(voteSig));
  BOOST_CHECK(esperanza::Vote::CheckSignature(pk, vote, voteSig));
}

BOOST_FIXTURE_TEST_CASE(vote_signature2, TestingSetup) {

  CBasicKeyStore keystore;

  auto params = blockchain::Behavior::NewFromParameters(blockchain::Parameters::TestNet());

  CTxDestination dest = DecodeDestination("muUx4dQ4bwssNQYpUqAJHSJCUonAZ4Ro2s", *params);
  const CKeyID *keyID = boost::get<CKeyID>(&dest);

  esperanza::Vote vote{*keyID, uint256S("4e7eae1625c033a05e92cff8d1591e4c7511888c264dbc8917ef94c3e66f22ef"), 12, 13};

  std::string pkey = "cNJWVLVrfrxZT85cwYfHdbRKGi2FQjkKFBjocwwinNNix5tytG33";

  CUnitESecret vchSecret;
  vchSecret.SetString(pkey);
  CKey key = vchSecret.GetKey();
  keystore.AddKey(key);

  std::vector<unsigned char> voteSig;
  esperanza::Vote::CreateSignature(&keystore, vote, voteSig);
}

BOOST_AUTO_TEST_SUITE_END()
