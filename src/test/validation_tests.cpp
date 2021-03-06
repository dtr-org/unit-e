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
#include <test/util/txtools.h>
#include <test/util/util.h>

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
struct Fixture : public txtools::TxTool {
  std::unique_ptr<blockchain::Behavior> blockchain_behavior =
      blockchain::Behavior::NewForNetwork(blockchain::Network::test);
  mocks::ActiveChainMock active_chain;
  std::unique_ptr<staking::BlockValidator> block_validator =
      staking::BlockValidator::New(blockchain_behavior.get());
  mocks::NetworkMock network;
  std::unique_ptr<staking::LegacyValidationInterface> validation;

  const CChainParams &chainparams;
  const Consensus::Params &params;

  explicit Fixture(decltype(&staking::LegacyValidationInterface::LegacyImpl) factory)
      : validation(factory(&active_chain, block_validator.get(), &network)),
        chainparams(Params()),
        params(chainparams.GetConsensus()) {}
};
struct LegacyImpl : public Fixture {
  LegacyImpl() : Fixture(staking::LegacyValidationInterface::LegacyImpl) {}
};
struct NewImpl : public Fixture {
  NewImpl() : Fixture(staking::LegacyValidationInterface::New) {}
};
using TestFixtures = boost::mpl::list<LegacyImpl, NewImpl>;

}  // namespace

BOOST_FIXTURE_TEST_SUITE(validation_tests, TestingSetup)

CMutableTransaction CreateTx(const TxType txtype = TxType::REGULAR) {

  CMutableTransaction mut_tx;
  mut_tx.SetType(txtype);

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

CMutableTransaction CreateCoinbase(blockchain::Height height = 0) {
  CMutableTransaction coinbase_tx;
  coinbase_tx.SetType(TxType::COINBASE);
  coinbase_tx.vin.resize(2);
  coinbase_tx.vin[0].prevout.SetNull();
  coinbase_tx.vin[1].prevout = {uint256::zero, 2};
  coinbase_tx.vout.resize(1);
  coinbase_tx.vout[0].scriptPubKey = CScript();
  coinbase_tx.vout[0].nValue = 0;
  coinbase_tx.vin[0].scriptSig = CScript() << CScriptNum::serialize(height) << ToByteVector(GetRandHash());
  return coinbase_tx;
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_empty, F, TestFixtures) {
  F fixture;

  CBlock block;
  assert(block.vtx.empty());

  CValidationState state;
  fixture.validation->CheckBlock(block, state, fixture.params, false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-length");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_too_many_transactions, F, TestFixtures) {
  F fixture;

  auto tx_weight = GetTransactionWeight(CTransaction(CreateTx()));

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateTx(TxType::COINBASE)));
  for (int i = 0; i <= (MAX_BLOCK_WEIGHT / tx_weight * WITNESS_SCALE_FACTOR) + 1; ++i) {
    block.vtx.push_back(MakeTransactionRef(CreateTx()));
  }

  CValidationState state;
  fixture.validation->CheckBlock(block, state, fixture.params, false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-length");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_coinbase_missing, F, TestFixtures) {
  F fixture;

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CTransaction(CreateTx())));

  CValidationState state;
  fixture.validation->CheckBlock(block, state, fixture.params, false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-cb-missing");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_duplicate_coinbase, F, TestFixtures) {
  F fixture;

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));
  block.vtx.push_back(MakeTransactionRef(CTransaction(CreateTx())));
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));

  CValidationState state;
  fixture.validation->CheckBlock(block, state, fixture.params, false);

  ltor::SortTransactions(block.vtx);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-cb-multiple");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_too_many_sigs, F, TestFixtures) {
  F fixture;

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));

  auto tx = CreateTx();
  auto many_checksigs = CScript();
  for (int i = 0; i < (MAX_BLOCK_SIGOPS_COST / WITNESS_SCALE_FACTOR) + 1; ++i) {
    many_checksigs = many_checksigs << OP_CHECKSIG;
  }

  tx.vout[0].scriptPubKey = many_checksigs;
  block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));

  ltor::SortTransactions(block.vtx);

  CValidationState state;
  fixture.validation->CheckBlock(block, state, fixture.params, false);

  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-sigops");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_merkle_root, F, TestFixtures) {
  F fixture;

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));

  block.hashMerkleRoot = GetRandHash();

  CValidationState state;
  fixture.validation->CheckBlock(block, state, fixture.params, true);

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

  ltor::SortTransactions(block.vtx);

  bool ignored;
  block.hashMerkleRoot = BlockMerkleRoot(block, &ignored);

  CValidationState state;
  fixture.validation->CheckBlock(block, state, fixture.params, true);

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
  fixture.validation->CheckBlock(block, state, fixture.params, false);

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
  fixture.validation->CheckBlock(block, state, fixture.params, false);

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
    block.vtx.push_back(MakeTransactionRef(CreateCoinbase(prev.nHeight + 1)));
    block.vtx.push_back(MakeTransactionRef(final_tx));

    auto not_final_height_tx = CreateTx();
    not_final_height_tx.vin.resize(1);
    not_final_height_tx.vin[0].nSequence = 0;
    not_final_height_tx.nLockTime = 12;
    block.vtx.push_back(MakeTransactionRef(not_final_height_tx));
    SortTxs(block);

    CValidationState state;
    state.GetBlockValidationInfo().MarkCheckBlockSuccessfull(prev.nHeight + 1, uint256::zero);
    state.GetBlockValidationInfo().MarkContextualCheckBlockHeaderSuccessfull();
    fixture.validation->ContextualCheckBlock(block, state, fixture.params, &prev);

    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-nonfinal");
  }

  //test with a tx non final because of time
  {
    CBlock block;
    block.vtx.push_back(MakeTransactionRef(CreateCoinbase(prev.nHeight + 1)));
    block.vtx.push_back(MakeTransactionRef(final_tx));

    auto not_final_time_tx = CreateTx();
    not_final_time_tx.vin.resize(1);
    not_final_time_tx.vin[0].nSequence = 0;
    not_final_time_tx.nLockTime = 500000001;
    block.vtx.push_back(MakeTransactionRef(not_final_time_tx));
    SortTxs(block);

    CValidationState state;
    state.GetBlockValidationInfo().MarkCheckBlockSuccessfull(prev.nHeight + 1, uint256::zero);
    state.GetBlockValidationInfo().MarkContextualCheckBlockHeaderSuccessfull();
    fixture.validation->ContextualCheckBlock(block, state, fixture.params, &prev);

    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-nonfinal");
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_witness, F, TestFixtures) {
  F fixture;

  CBlockIndex prev;

  //bad witness merkle not matching
  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CreateCoinbase()));
  block.ComputeMerkleTrees();
  block.hash_witness_merkle_root = GetRandHash();

  CValidationState state;
  fixture.validation->CheckBlock(block, state, fixture.params, true);

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
  state.GetBlockValidationInfo().MarkCheckBlockSuccessfull(1, uint256::zero);
  state.GetBlockValidationInfo().MarkContextualCheckBlockHeaderSuccessfull();
  fixture.validation->ContextualCheckBlock(block, state, fixture.params, &prev);

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

    {
      CValidationState state;
      BOOST_CHECK(fixture.validation->ContextualCheckBlockHeader(block, state, fixture.chainparams, &prev_2, adjusted_time));
    }

    {
      CValidationState state;
      block.nTime = 1999;  // 1 unit less than the median
      BOOST_CHECK(!fixture.validation->ContextualCheckBlockHeader(block, state, fixture.chainparams, &prev_2, adjusted_time));
      BOOST_CHECK_EQUAL(state.GetRejectReason(), "time-too-old");
    }
  }

  // Block time is too far in the future
  {
    blockchain::Parameters params = blockchain::Parameters::TestNet();

    int64_t adjusted_time = 0;
    CBlockIndex prev;
    CBlock block;
    block.nTime = adjusted_time + params.max_future_block_time_seconds;

    prev.phashBlock = &block.hashPrevBlock;

    {
      CValidationState state;
      BOOST_CHECK(fixture.validation->ContextualCheckBlockHeader(block, state, fixture.chainparams, &prev, adjusted_time));
    }

    {
      CValidationState state;
      block.nTime = adjusted_time + params.max_future_block_time_seconds + 1;
      BOOST_CHECK(!fixture.validation->ContextualCheckBlockHeader(block, state, fixture.chainparams, &prev, adjusted_time));
      BOOST_CHECK_EQUAL(state.GetRejectReason(), "time-too-new");
    }
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_minimal_complete_block, F, TestFixtures) {
  F fixture;

  CBlock block = MinimalBlock();

  // check that a minimal complete block without modifications passes
  CValidationState state;
  BOOST_REQUIRE(fixture.validation->CheckBlock(block, state, fixture.params, true));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_tx_no_inputs, F, TestFixtures) {
  F fixture;

  CBlock block = MinimalBlock([&](CBlock &block) {
    CMutableTransaction mtx(fixture.CreateTransaction());
    mtx.vin.clear();
    block.vtx.emplace_back(MakeTransactionRef(mtx));
  });

  CValidationState state;
  BOOST_REQUIRE_MESSAGE(!fixture.validation->CheckBlock(block, state, fixture.params, true), state.GetRejectReason());
  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-vin-empty");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_tx_no_outputs, F, TestFixtures) {
  F fixture;

  CBlock block = MinimalBlock([&](CBlock &block) {
    CMutableTransaction mtx(fixture.CreateTransaction());
    mtx.vout.clear();
    block.vtx.emplace_back(MakeTransactionRef(mtx));
  });

  CValidationState state;
  BOOST_REQUIRE_MESSAGE(!fixture.validation->CheckBlock(block, state, fixture.params, true), state.GetRejectReason());
  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-vout-empty");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_tx_duplicate_inputs, F, TestFixtures) {
  F fixture;

  CBlock block = MinimalBlock([&](CBlock &block) {
    CMutableTransaction mtx(fixture.CreateTransaction());
    mtx.vin.emplace_back(mtx.vin.back());
    block.vtx.emplace_back(MakeTransactionRef(mtx));
  });

  CValidationState state;
  BOOST_REQUIRE_MESSAGE(!fixture.validation->CheckBlock(block, state, fixture.params, true), state.GetRejectReason());
  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-inputs-duplicate");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_tx_negative_output, F, TestFixtures) {
  F fixture;

  CBlock block = MinimalBlock([&](CBlock &block) {
    CMutableTransaction mtx(fixture.CreateTransaction());
    CAmount out(-1);
    mtx.vout.emplace_back(out, CScript());
    block.vtx.emplace_back(MakeTransactionRef(mtx));
  });

  CValidationState state;
  BOOST_REQUIRE_MESSAGE(!fixture.validation->CheckBlock(block, state, fixture.params, true), state.GetRejectReason());
  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-vout-negative");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_tx_output_pays_too_much, F, TestFixtures) {
  F fixture;

  CBlock block = MinimalBlock([&](CBlock &block) {
    CMutableTransaction mtx(fixture.CreateTransaction());
    mtx.vout[0].nValue = fixture.blockchain_behavior->GetParameters().expected_maximum_supply + 1;
    block.vtx.emplace_back(MakeTransactionRef(mtx));
  });

  CValidationState state;
  BOOST_REQUIRE_MESSAGE(!fixture.validation->CheckBlock(block, state, fixture.params, true), state.GetRejectReason());
  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-vout-toolarge");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_tx_sum_of_outputs_pays_too_much, F, TestFixtures) {
  F fixture;

  CBlock block = MinimalBlock([&](CBlock &block) {
    CMutableTransaction mtx(fixture.CreateTransaction());
    CScript script_pub_key = mtx.vout[0].scriptPubKey;
    mtx.vout.clear();
    for (std::size_t ix = 0; ix < 2; ++ix) {
      const CAmount amount = fixture.blockchain_behavior->GetParameters().expected_maximum_supply - 1;
      mtx.vout.emplace_back(amount, script_pub_key);
    }
    block.vtx.emplace_back(MakeTransactionRef(mtx));
  });

  CValidationState state;
  BOOST_REQUIRE_MESSAGE(!fixture.validation->CheckBlock(block, state, fixture.params, true), state.GetRejectReason());
  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-txouttotal-toolarge");
}

BOOST_AUTO_TEST_CASE_TEMPLATE(checkblock_tx_null_input, F, TestFixtures) {
  F fixture;

  CBlock block = MinimalBlock([&](CBlock &block) {
    CMutableTransaction mtx(fixture.CreateTransaction());
    mtx.vin[0].prevout.SetNull();
    block.vtx.emplace_back(MakeTransactionRef(mtx));
  });

  CValidationState state;
  BOOST_REQUIRE_MESSAGE(!fixture.validation->CheckBlock(block, state, fixture.params, true), state.GetRejectReason());
  BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-prevout-null");
}

BOOST_AUTO_TEST_SUITE_END()
