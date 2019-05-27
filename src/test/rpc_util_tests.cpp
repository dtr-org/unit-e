// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/util.h>

#include <amount.h>
#include <extkey.h>
#include <key.h>
#include <key/mnemonic/mnemonic.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/standard.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

#include <univalue.h>

BOOST_FIXTURE_TEST_SUITE(rpc_util_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(ToUniValue_COutPoint_checks) {
  {
    const COutPoint outpoint;
    const UniValue value = ToUniValue(outpoint);
    const std::string result = value.write();
    const std::string expected =
        R"({"txid":"0000000000000000000000000000000000000000000000000000000000000000","n":4294967295})";
    BOOST_CHECK_EQUAL(result, expected);
  }
  {
    const COutPoint outpoint(uint256S("a31722b71a400186eefc0c422ec5f931b86a33276034a387986234c87f4a63fd"), 17);
    const UniValue value = ToUniValue(outpoint);
    const std::string result = value.write();
    const std::string expected =
        R"({"txid":"a31722b71a400186eefc0c422ec5f931b86a33276034a387986234c87f4a63fd","n":17})";
    BOOST_CHECK_EQUAL(result, expected);
  }
}

BOOST_AUTO_TEST_CASE(ToUniValue_CTxOut_checks) {
  const key::mnemonic::Seed seed("cook note face vicious suggest company unit smart lobster tongue dune diamond faculty solid thought");
  const CExtKey &ext_key = seed.GetExtKey();
  const CPubKey pub_key = ext_key.key.GetPubKey();
  const CTxDestination destination = pub_key.GetID();
  const CScript script_pub_key = GetScriptForDestination(destination);

  const CTxOut txout(10 * UNIT, script_pub_key);
  const UniValue value = ToUniValue(txout);
  const std::string result = value.write(2, 0);
  const std::string expected =
      R"({
  "amount": 10.00000000,
  "scriptPubKey": {
    "asm": "OP_DUP OP_HASH160 6b2bce0cda70323b919f64eabac27f53167533fb OP_EQUALVERIFY OP_CHECKSIG",
    "hex": "76a9146b2bce0cda70323b919f64eabac27f53167533fb88ac",
    "reqSigs": 1,
    "type": "pubkeyhash",
    "addresses": [
      "mqHd5CMScY2h1NZbZg5zMdYU7ezH8P3mWc"
    ]
  }
})";
  BOOST_CHECK_EQUAL(result, expected);
}

BOOST_AUTO_TEST_SUITE_END()
