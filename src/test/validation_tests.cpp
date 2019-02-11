// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <keystore.h>
#include <script/interpreter.h>
#include <test/test_unite.h>
#include <validation.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(validation_tests, TestingSetup)

CMutableTransaction CreateTx() {

  CMutableTransaction mut_tx;

  CBasicKeyStore keystore;
  CKey k;
  InsecureNewKey(k, true);
  keystore.AddKey(k);

  mut_tx.vin.emplace_back(GetRandHash(), 0);
  mut_tx.vin.emplace_back(GetRandHash(), 0);
  mut_tx.vin.emplace_back(GetRandHash(), 0);
  mut_tx.vin.emplace_back(GetRandHash(), 0);

  CTxOut out(100 * UNIT, CScript::CreateP2PKHScript(std::vector<unsigned char>(20)));
  mut_tx.vout.push_back(out);
  mut_tx.vout.push_back(out);
  mut_tx.vout.push_back(out);
  mut_tx.vout.push_back(out);

  // Sign
  std::vector<unsigned char> vchSig(20);
  uint256 hash = SignatureHash(CScript(), mut_tx, 0,
                               SIGHASH_ALL, 0, SigVersion::BASE);

  BOOST_CHECK(k.Sign(hash, vchSig));
  vchSig.push_back((unsigned char)SIGHASH_ALL);

  mut_tx.vin[0].scriptSig = CScript() << ToByteVector(vchSig)
                                     << ToByteVector(k.GetPubKey());

  return mut_tx;
}

CTransaction CreateCoinbase() {
  CMutableTransaction coinbase_tx;
  coinbase_tx.vin.resize(1);
  coinbase_tx.vin[0].prevout.SetNull();
  coinbase_tx.vout.resize(1);
  coinbase_tx.vout[0].scriptPubKey = CScript();
  coinbase_tx.vout[0].nValue = 0;
  coinbase_tx.vin[0].scriptSig = CScript() << CScriptNum::serialize(0) << ToByteVector(GetRandHash());
  return coinbase_tx;
}

BOOST_AUTO_TEST_CASE(checkblock_empty) {

  CBlock block;
  assert(block.vtx.empty());

  CValidationState state;
  CheckBlock(block, state, Params().GetConsensus(), false, false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-length");
}

BOOST_AUTO_TEST_CASE(checkblock_too_many_transactions) {

  auto tx_weight = GetTransactionWeight(CTransaction(CreateTx()));

  CBlock block;
  for (int i = 0; i <= (MAX_BLOCK_WEIGHT / tx_weight * WITNESS_SCALE_FACTOR) + 1; ++i) {
    block.vtx.push_back(MakeTransactionRef(CreateTx()));
  }

  CValidationState state;
  CheckBlock(block, state, Params().GetConsensus(), false, false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-length");
}

BOOST_AUTO_TEST_CASE(checkblock_coinbase_missing) {

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CTransaction(CreateTx())));

  CValidationState state;
  CheckBlock(block, state, Params().GetConsensus(), false, false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-cb-missing");
}

BOOST_AUTO_TEST_CASE(checkblock_duplicate_coinbase) {

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));
  block.vtx.push_back(MakeTransactionRef(CTransaction(CreateTx())));
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));

  CValidationState state;
  CheckBlock(block, state, Params().GetConsensus(), false, false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-cb-multiple");
}

BOOST_AUTO_TEST_CASE(checkblock_too_many_sigs) {

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));

  auto tx = CreateTx();
  auto many_checsigs = CScript();
  for (int i = 0; i < (MAX_BLOCK_SIGOPS_COST / WITNESS_SCALE_FACTOR) + 1; ++i) {
    many_checsigs = many_checsigs << OP_CHECKSIG;
  }

  tx.vout[0].scriptPubKey = many_checsigs;
  block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));

  CValidationState state;
  CheckBlock(block, state, Params().GetConsensus(), false, false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-sigops");
}

BOOST_AUTO_TEST_CASE(checkblock_merkle_root) {
  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));

  block.hashMerkleRoot = GetRandHash();

  CValidationState state;
  CheckBlock(block, state, Params().GetConsensus(), false, true);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txnmrklroot");
}

BOOST_AUTO_TEST_CASE(checkblock_merkle_root_mutated) {

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));
  auto tx = CTransaction(CreateTx());
  block.vtx.push_back(MakeTransactionRef(CreateTx()));
  block.vtx.push_back(MakeTransactionRef(tx));
  block.vtx.push_back(MakeTransactionRef(tx));

  bool ignored;
  block.hashMerkleRoot = BlockMerkleRoot(block, &ignored);

  CValidationState state;
  CheckBlock(block, state, Params().GetConsensus(), false, true);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-duplicate");
}

BOOST_AUTO_TEST_SUITE_END()
