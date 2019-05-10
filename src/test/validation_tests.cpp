// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/ltor.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <keystore.h>
#include <script/interpreter.h>
#include <staking/legacy_validation_interface.h>
#include <validation.h>

#include <test/test_unite.h>
#include <test/test_unite_mocks.h>

#include <boost/mpl/list.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/test/unit_test.hpp>

namespace {
void SortTxs(CBlock &block, bool reverse = false) {
  ltor::SortTransactions(block.vtx);
  if (reverse) {
    std::reverse(block.vtx.begin() + 1, block.vtx.end());
  }
}
struct Fixture {
  std::unique_ptr<blockchain::Behavior> blockchain_behavior =
      blockchain::Behavior::NewForNetwork(blockchain::Network::test);
  mocks::ActiveChainMock active_chain;
  std::unique_ptr<staking::BlockValidator> block_validator =
      staking::BlockValidator::New(blockchain_behavior.get());
  mocks::StakeValidatorMock stake_validator;
  std::unique_ptr<staking::LegacyValidationInterface> validation;

  explicit Fixture(decltype(&staking::LegacyValidationInterface::LegacyImpl) factory)
      : validation(factory(&active_chain, block_validator.get(), &stake_validator)) {}
};
struct LegacyImpl : public Fixture {
  LegacyImpl() : Fixture(staking::LegacyValidationInterface::LegacyImpl) {}
};
using TestFixtures = boost::mpl::list<LegacyImpl>;

}  // namespace

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

CMutableTransaction CreateCoinbase() {
  CMutableTransaction coinbase_tx;
  coinbase_tx.SetType(TxType::COINBASE);
  coinbase_tx.vin.resize(1);
  coinbase_tx.vin[0].prevout.SetNull();
  coinbase_tx.vout.resize(1);
  coinbase_tx.vout[0].scriptPubKey = CScript();
  coinbase_tx.vout[0].nValue = 0;
  coinbase_tx.vin[0].scriptSig = CScript() << CScriptNum::serialize(0) << ToByteVector(GetRandHash());
  return coinbase_tx;
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_empty, F, TestFixtures) {
  F fixture;

  CBlock block;
  assert(block.vtx.empty());

  CValidationState state;
  fixture.validation->CheckBlock(block, state, Params().GetConsensus(), false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-length");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_too_many_transactions, F, TestFixtures) {
  F fixture;

  auto tx_weight = GetTransactionWeight(CTransaction(CreateTx()));

  CBlock block;
  for (int i = 0; i <= (MAX_BLOCK_WEIGHT / tx_weight * WITNESS_SCALE_FACTOR) + 1; ++i) {
    block.vtx.push_back(MakeTransactionRef(CreateTx()));
  }

  CValidationState state;
  fixture.validation->CheckBlock(block, state, Params().GetConsensus(), false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-length");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_coinbase_missing, F, TestFixtures) {
  F fixture;

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CTransaction(CreateTx())));

  CValidationState state;
  fixture.validation->CheckBlock(block, state, Params().GetConsensus(), false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-cb-missing");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_duplicate_coinbase, F, TestFixtures) {
  F fixture;

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));
  block.vtx.push_back(MakeTransactionRef(CTransaction(CreateTx())));
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));

  CValidationState state;
  fixture.validation->CheckBlock(block, state, Params().GetConsensus(), false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-cb-multiple");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_too_many_sigs, F, TestFixtures) {
  F fixture;

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
  fixture.validation->CheckBlock(block, state, Params().GetConsensus(), false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-sigops");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_merkle_root, F, TestFixtures) {
  F fixture;

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));

  block.hashMerkleRoot = GetRandHash();

  CValidationState state;
  fixture.validation->CheckBlock(block, state, Params().GetConsensus(), true);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txnmrklroot");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_merkle_root_mutated, F, TestFixtures) {
  F fixture;

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));
  auto tx = CTransaction(CreateTx());
  block.vtx.push_back(MakeTransactionRef(CreateTx()));
  block.vtx.push_back(MakeTransactionRef(tx));
  block.vtx.push_back(MakeTransactionRef(tx));

  bool ignored;
  block.hashMerkleRoot = BlockMerkleRoot(block, &ignored);

  CValidationState state;
  fixture.validation->CheckBlock(block, state, Params().GetConsensus(), true);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-duplicate");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_duplicates_tx, F, TestFixtures) {
  F fixture;

  CBlockIndex prev;
  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));

  auto tx = CreateTx();
  block.vtx.push_back(MakeTransactionRef(tx));
  block.vtx.push_back(MakeTransactionRef(tx));

  CValidationState state;
  fixture.validation->CheckBlock(block, state, Params().GetConsensus(), false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-duplicate");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_tx_order, F, TestFixtures) {
  F fixture;

  CBlockIndex prev;
  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));
  block.vtx.push_back(MakeTransactionRef(CreateTx()));
  block.vtx.push_back(MakeTransactionRef(CreateTx()));
  SortTxs(block, true);

  CValidationState state;
  fixture.validation->CheckBlock(block, state, Params().GetConsensus(), false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-tx-ordering");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(contextualcheckblock_is_final_tx, F, TestFixtures) {
  F fixture;

  CBlockIndex prev;
  prev.nTime = 100000;
  prev.nHeight = 10;

  CMutableTransaction final_tx = CreateTx();
  final_tx.nLockTime = 0;
  final_tx.vin.resize(1);
  final_tx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;

  //test with a tx non final because of height
  {
    CBlock block;
    block.vtx.push_back(MakeTransactionRef(final_tx));

    auto not_final_height_tx = CreateTx();
    not_final_height_tx.vin.resize(1);
    not_final_height_tx.vin[0].nSequence = 0;
    not_final_height_tx.nLockTime = 12;
    block.vtx.push_back(MakeTransactionRef(not_final_height_tx));
    SortTxs(block);

    CValidationState state;
    fixture.validation->ContextualCheckBlock(block, state, Params().GetConsensus(), &prev);

    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-nonfinal");
  }

  //test with a tx non final because of time
  {
    CBlock block;
    block.vtx.push_back(MakeTransactionRef(final_tx));

    auto not_final_time_tx = CreateTx();
    not_final_time_tx.vin.resize(1);
    not_final_time_tx.vin[0].nSequence = 0;
    not_final_time_tx.nLockTime = 500000001;
    block.vtx.push_back(MakeTransactionRef(not_final_time_tx));
    SortTxs(block);

    CValidationState state;
    fixture.validation->ContextualCheckBlock(block, state, Params().GetConsensus(), &prev);

    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-nonfinal");
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_witness, F, TestFixtures) {
  F fixture;

  CBlockIndex prev;

  auto consensus_params = Params().GetConsensus();

  //bad witness merkle not matching
  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));
  block.ComputeMerkleTrees();
  block.hash_witness_merkle_root = GetRandHash();

  CValidationState state;
  fixture.validation->CheckBlock(block, state, consensus_params, true);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-witness-merkle-match");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(contextualcheckblock_block_weight, F, TestFixtures) {
  F fixture;

  CBlockIndex prev;
  CBlock block;
  for (int i = 0; i < 5000; ++i) {
    block.vtx.push_back(MakeTransactionRef(CreateTx()));
    block.vtx.push_back(MakeTransactionRef(CreateTx()));
  }
  SortTxs(block);

  CValidationState state;
  fixture.validation->ContextualCheckBlock(block, state, Params().GetConsensus(), &prev);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-weight");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(contextualcheckblockheader_time, F, TestFixtures) {
  F fixture;

  // Block time is too far in the past
  int64_t adjusted_time = 151230;
  {
    // Setup prev chain
    CBlockIndex prev_0;
    CBlockIndex prev_1;
    CBlockIndex prev_2;

    prev_0.nTime = 1000;
    prev_1.nTime = 2000;
    prev_2.nTime = 3000;

    prev_1.pprev = &prev_0;
    prev_2.pprev = &prev_1;

    CBlock block;
    block.nTime = 2001;  // 1 unit more than the median

    prev_2.phashBlock = &block.hashPrevBlock;

    CValidationState state;
    BOOST_CHECK(fixture.validation->ContextualCheckBlockHeader(block, state, Params(), &prev_2, adjusted_time));

    block.nTime = 1999;  // 1 unit less than the median
    fixture.validation->ContextualCheckBlockHeader(block, state, Params(), &prev_2, adjusted_time);
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "time-too-old");
  }

  // Block time is too far in the future
  {
    blockchain::Parameters params = blockchain::Parameters::TestNet();

    int64_t adjusted_time = 0;
    CBlockIndex prev;
    CBlock block;
    block.nTime = adjusted_time + params.max_future_block_time_seconds;

    prev.phashBlock = &block.hashPrevBlock;

    CValidationState state;
    BOOST_CHECK(fixture.validation->ContextualCheckBlockHeader(block, state, Params(), &prev, adjusted_time));

    block.nTime = adjusted_time + params.max_future_block_time_seconds + 1;
    fixture.validation->ContextualCheckBlockHeader(block, state, Params(), &prev, adjusted_time);
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "time-too-new");
  }
}

BOOST_AUTO_TEST_SUITE_END()
