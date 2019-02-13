// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/block_validator.h>

#include <blockchain/blockchain_genesis.h>
#include <blockchain/blockchain_parameters.h>
#include <consensus/merkle.h>
#include <key/mnemonic/mnemonic.h>
#include <test/test_unite.h>
#include <timedata.h>

#include <boost/test/unit_test.hpp>

namespace {

std::unique_ptr<blockchain::Behavior> b =
    blockchain::Behavior::NewFromParameters(blockchain::Parameters::MainNet());

//! \brief creates a minimal block that passes validation without looking at the chain
CBlock MinimalBlock() {
  // a block is signed by the proposer, thus we need some key setup here
  const key::mnemonic::Seed seed("cook note face vicious suggest company unit smart lobster tongue dune diamond faculty solid thought");
  const CExtKey &ext_key = seed.GetExtKey();
  // public key for signing block
  const CPubKey pub_key = ext_key.key.GetPubKey();
  const auto pub_key_data = std::vector<unsigned char>(pub_key.begin(), pub_key.end());

  CBlock block;
  block.nTime = b->CalculateProposingTimestamp(std::time(nullptr));
  {
    CMutableTransaction tx;
    tx.SetType(TxType::COINBASE);
    // meta input: block height, snapshot hash, terminator
    CScript script_sig = CScript() << CScriptNum::serialize(4711)
                                   << ToByteVector(uint256S("689dae90b6913ff34a64750dd537177afa58b3d012803a10793d74f1ebb88da9"));
    tx.vin.emplace_back(uint256(), 0, script_sig);
    // stake
    tx.vin.emplace_back(uint256(), 1);
    tx.vin[1].scriptWitness.stack.emplace_back();  // signature, not checked
    tx.vin[1].scriptWitness.stack.emplace_back(pub_key_data);
    // can be spent by anyone, simply yields "true"
    CScript script_pub_key = CScript() << OP_TRUE;
    tx.vout.emplace_back(50, script_pub_key);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }
  {
    CMutableTransaction tx;
    tx.SetType(TxType::STANDARD);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }

  bool duplicate_transactions;
  block.hashMerkleRoot = BlockMerkleRoot(block, &duplicate_transactions);
  block.hash_witness_merkle_root = BlockWitnessMerkleRoot(block, &duplicate_transactions);
  const uint256 blockHash = block.GetHash();

  ext_key.key.Sign(blockHash, block.signature);

  return block;
}

void CheckGenesisBlock(const blockchain::Parameters &parameters) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  const auto validation_result = block_validator->CheckBlock(parameters.genesis_block->block, nullptr);

  BOOST_CHECK(static_cast<bool>(validation_result));
}

}  // namespace

BOOST_FIXTURE_TEST_SUITE(block_validator_tests, BasicTestingSetup)

using Error = staking::BlockValidationError;

BOOST_AUTO_TEST_CASE(check_empty_block) {
  const auto block_validator = staking::BlockValidator::New(b.get());

  CBlock block;

  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK(validation_result.errors.Contains(Error::NO_TRANSACTIONS));
}

BOOST_AUTO_TEST_CASE(check_first_transaction_not_a_coinbase_transaction) {
  const auto block_validator = staking::BlockValidator::New(b.get());

  CMutableTransaction tx;
  tx.SetType(TxType::STANDARD);

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));

  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK(validation_result.errors.Contains(Error::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION));
  BOOST_CHECK(!validation_result.errors.Contains(Error::COINBASE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST));
}

BOOST_AUTO_TEST_CASE(check_coinbase_other_than_first) {
  const auto block_validator = staking::BlockValidator::New(b.get());

  CBlock block;
  {
    CMutableTransaction tx;
    tx.SetType(TxType::STANDARD);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }
  {
    CMutableTransaction tx;
    tx.SetType(TxType::COINBASE);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }

  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK(validation_result.errors.Contains(Error::COINBASE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST));
  BOOST_CHECK(validation_result.errors.Contains(Error::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION));
}

BOOST_AUTO_TEST_CASE(check_two_coinbase_transactions) {
  const auto block_validator = staking::BlockValidator::New(b.get());

  CBlock block;
  {
    CMutableTransaction tx;
    tx.SetType(TxType::COINBASE);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }
  {
    CMutableTransaction tx;
    tx.SetType(TxType::COINBASE);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }

  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(validation_result.errors.Contains(Error::COINBASE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST));
  BOOST_CHECK(!validation_result.errors.Contains(Error::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION));
  BOOST_CHECK(!validation_result);
}

BOOST_AUTO_TEST_CASE(check_NO_block_height) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  coinbase.vin[0].scriptSig = CScript();
  block.vtx[0] = MakeTransactionRef(coinbase);

  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
}

BOOST_AUTO_TEST_CASE(check_premature_end_of_scriptsig) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  coinbase.vin[0].scriptSig = CScript() << CScriptNum::serialize(4711)
                                        << OP_0;
  block.vtx[0] = MakeTransactionRef(coinbase);

  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result.errors.Contains(Error::NO_BLOCK_HEIGHT));
  BOOST_CHECK(validation_result.errors.Contains(Error::NO_SNAPSHOT_HASH));
  BOOST_CHECK(!validation_result);
}

BOOST_AUTO_TEST_CASE(check_scriptsig_with_additional_data) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  coinbase.vin[0].scriptSig = CScript() << CScriptNum::serialize(4711)
                                        << ToByteVector(uint256())
                                        << ToByteVector(uint256());
  block.vtx[0] = MakeTransactionRef(coinbase);

  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result.errors.Contains(Error::NO_BLOCK_HEIGHT));
  BOOST_CHECK(!validation_result.errors.Contains(Error::NO_SNAPSHOT_HASH));
}

BOOST_AUTO_TEST_CASE(check_NO_snapshot_hash) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  coinbase.vin[0].scriptSig = CScript() << CScriptNum::serialize(7) << OP_0;
  block.vtx[0] = MakeTransactionRef(coinbase);

  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result.errors.Contains(Error::NO_BLOCK_HEIGHT));
  BOOST_CHECK(validation_result.errors.Contains(Error::NO_SNAPSHOT_HASH));
  BOOST_CHECK(!validation_result);
}

BOOST_AUTO_TEST_CASE(check_empty_coinbase_transaction) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  // empty coinbase transaction
  auto coinbase = CMutableTransaction();
  coinbase.SetType(TxType::COINBASE);
  block.vtx[0] = MakeTransactionRef(coinbase);

  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(validation_result.errors.Contains(Error::NO_META_INPUT));
  BOOST_CHECK(validation_result.errors.Contains(Error::COINBASE_TRANSACTION_WITHOUT_OUTPUT));
  BOOST_CHECK(validation_result.errors.Contains(Error::NO_STAKING_INPUT));
  BOOST_CHECK(!validation_result);
}

BOOST_AUTO_TEST_CASE(check_coinbase_transaction_without_stake) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  // remove coin stake input
  coinbase.vin.erase(coinbase.vin.begin() + 1);
  block.vtx[0] = MakeTransactionRef(coinbase);

  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(validation_result.errors.Contains(Error::NO_STAKING_INPUT));
  BOOST_CHECK(!validation_result);
}

BOOST_AUTO_TEST_CASE(no_public_key) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  // remove public key from staking input's witness stack
  coinbase.vin[1].scriptWitness.stack.clear();
  block.vtx[0] = MakeTransactionRef(coinbase);
  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(validation_result.errors.Contains(Error::INVALID_BLOCK_PUBLIC_KEY));
  BOOST_CHECK(!validation_result);
}

BOOST_AUTO_TEST_CASE(invalid_block_signature) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  // corrupt signature by flipping some byte
  block.signature[7] = ~block.signature[7];
  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK(validation_result.errors.Contains(Error::BLOCK_SIGNATURE_VERIFICATION_FAILED));
}

BOOST_AUTO_TEST_CASE(invalid_block_time) {
  // UNIT-E TODO: Temporarily disabled to fix old bitcoin tests
//  const auto block_validator = staking::BlockValidator::New(b.get());
//  CBlock block = MinimalBlock();
//  // corrupt block time by offsetting it by 1
//  block.nTime = block.nTime + 1;
//  const auto validation_result = block_validator->CheckBlock(block, nullptr);
//
//  BOOST_CHECK(!validation_result);
//  BOOST_CHECK(validation_result.errors.Contains(Error::INVALID_BLOCK_TIME));
}

BOOST_AUTO_TEST_CASE(valid_block) {
  staking::BlockValidationInfo block_validation_info;

  const auto block_validator = staking::BlockValidator::New(b.get());
  const auto validation_result = block_validator->CheckBlock(MinimalBlock(), &block_validation_info);

  BOOST_CHECK(static_cast<bool>(validation_result));
  BOOST_CHECK(static_cast<bool>(block_validation_info.GetCheckBlockStatus()));

  const blockchain::Height expected_height = 4711;
  const uint256 expected_snapshot_hash =
      uint256S("689dae90b6913ff34a64750dd537177afa58b3d012803a10793d74f1ebb88da9");

  BOOST_CHECK_EQUAL(validation_result.GetRejectionMessage(), "");
  BOOST_CHECK_EQUAL(block_validation_info.GetSnapshotHash(), expected_snapshot_hash);
  BOOST_CHECK_EQUAL(block_validation_info.GetHeight(), expected_height);
}

BOOST_AUTO_TEST_CASE(genesis_block_mainnet) {
  CheckGenesisBlock(blockchain::Parameters::MainNet());
}

BOOST_AUTO_TEST_CASE(genesis_block_testnet) {
  CheckGenesisBlock(blockchain::Parameters::TestNet());
}

BOOST_AUTO_TEST_CASE(genesis_block_regtest) {
  CheckGenesisBlock(blockchain::Parameters::RegTest());
}

BOOST_AUTO_TEST_SUITE_END()
