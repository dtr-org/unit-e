// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
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
#include <boost/test/unit_test.hpp>

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
}

BOOST_AUTO_TEST_SUITE_END()
