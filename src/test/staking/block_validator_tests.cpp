// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/block_validator.h>

#include <blockchain/blockchain_genesis.h>
#include <blockchain/blockchain_parameters.h>
#include <consensus/merkle.h>
#include <key/mnemonic/mnemonic.h>
#include <test/test_unite.h>
#include <test/util/blocktools.h>
#include <timedata.h>

#include <boost/test/unit_test.hpp>

namespace {

std::unique_ptr<blockchain::Behavior> b =
    blockchain::Behavior::NewFromParameters(blockchain::Parameters::TestNet());

struct KeyFixture {
  CExtKey ext_key;
  CPubKey pub_key;
  std::vector<unsigned char> pub_key_data;
};

KeyFixture MakeKeyFixture(const std::string &seed_words = "cook note face vicious suggest company unit smart lobster tongue dune diamond faculty solid thought") {
  // a block is signed by the proposer, thus we need some key setup here
  const key::mnemonic::Seed seed(seed_words);
  const CExtKey &ext_key = seed.GetExtKey();
  // public key for signing block
  const CPubKey pub_key = ext_key.key.GetPubKey();
  return {
      ext_key,
      pub_key,
      std::vector<unsigned char>(pub_key.begin(), pub_key.end())};
}

CTransactionRef MakeCoinbaseTransaction(const KeyFixture &key_fixture = MakeKeyFixture(), const blockchain::Height height = 0) {

  CMutableTransaction tx;
  tx.SetType(TxType::COINBASE);

  // meta input: block height, snapshot hash, terminator
  CScript script_sig = CScript() << CScriptNum::serialize(4711)
                                 << ToByteVector(uint256S("689dae90b6913ff34a64750dd537177afa58b3d012803a10793d74f1ebb88da9"));
  tx.vin.emplace_back(COutPoint(), script_sig);
  // stake
  tx.vin.emplace_back(uint256(), 1);
  tx.vin[1].scriptWitness.stack.emplace_back();  // signature, not checked
  tx.vin[1].scriptWitness.stack.emplace_back(key_fixture.pub_key_data);
  // can be spent by anyone, simply yields "true"
  CScript script_pub_key = CScript() << OP_TRUE;
  tx.vout.emplace_back(50, script_pub_key);
  return MakeTransactionRef(CTransaction(tx));
}

//! \brief creates a minimal block that passes validation without looking at the chain
CBlock MinimalBlock(const KeyFixture &key_fixture = MakeKeyFixture()) {
  // a block is signed by the proposer, thus we need some key setup here
  const key::mnemonic::Seed seed("cook note face vicious suggest company unit smart lobster tongue dune diamond faculty solid thought");
  const CExtKey &ext_key = seed.GetExtKey();
  // public key for signing block
  const CPubKey pub_key = ext_key.key.GetPubKey();
  const auto pub_key_data = std::vector<unsigned char>(pub_key.begin(), pub_key.end());

  CBlock block;
  block.nTime = b->CalculateProposingTimestamp(std::time(nullptr));
  block.vtx.emplace_back(MakeCoinbaseTransaction(key_fixture));
  {
    CMutableTransaction tx;
    tx.SetType(TxType::REGULAR);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }
  block.hashMerkleRoot = BlockMerkleRoot(block);
  block.hash_witness_merkle_root = BlockWitnessMerkleRoot(block);
  block.hash_finalizer_commits_merkle_root = BlockFinalizerCommitsMerkleRoot(block);
  const uint256 blockHash = block.GetHash();

  ext_key.key.Sign(blockHash, block.signature);

  return block;
}

void CheckGenesisBlock(const blockchain::Parameters &parameters) {
  // the behaviour has to be from the correct parameters,
  // as the genesis block differs for each of them
  std::unique_ptr<blockchain::Behavior> chain_behaviour =
      blockchain::Behavior::NewFromParameters(parameters);
  const auto block_validator = staking::BlockValidator::New(chain_behaviour.get());
  const auto validation_result = block_validator->CheckBlock(parameters.genesis_block.block, nullptr);

  BOOST_CHECK_MESSAGE(static_cast<bool>(validation_result), validation_result.GetRejectionMessage());
}

using Error = staking::BlockValidationError;

}  // namespace

BOOST_FIXTURE_TEST_SUITE(block_validator_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(check_empty_block) {
  const auto block_validator = staking::BlockValidator::New(b.get());

  CBlock block;

  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::NO_TRANSACTIONS), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(check_first_transaction_not_a_coinbase_transaction) {
  // checks a block that lacks a coinbase transaction
  const auto block_validator = staking::BlockValidator::New(b.get());

  CMutableTransaction tx;
  tx.SetType(TxType::REGULAR);

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));

  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(check_coinbase_meta_input_malformed) {
  const auto block_validator = staking::BlockValidator::New(b.get());

  CBlock block;
  {
    CMutableTransaction tx;
    tx.SetType(TxType::COINBASE);
    tx.vin.emplace_back(uint256::zero, 0);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }

  const auto validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::INVALID_META_INPUT_PREVOUT), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(check_coinbase_other_than_first) {
  // checks a block that _has_ a coinbase transaction but not at the right position
  const auto block_validator = staking::BlockValidator::New(b.get());

  CBlock block;
  {
    CMutableTransaction tx;
    tx.SetType(TxType::REGULAR);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }
  block.vtx.emplace_back(MakeCoinbaseTransaction());

  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(check_two_coinbase_transactions) {
  const auto block_validator = staking::BlockValidator::New(b.get());

  CBlock block;
  const KeyFixture key_fixture = MakeKeyFixture();
  block.vtx.emplace_back(MakeCoinbaseTransaction(key_fixture));
  block.vtx.emplace_back(MakeCoinbaseTransaction(key_fixture));

  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::COINBASE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(check_NO_block_height) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  coinbase.vin[0].scriptSig = CScript();
  block.vtx[0] = MakeTransactionRef(coinbase);

  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::NO_BLOCK_HEIGHT), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(check_premature_end_of_scriptsig) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  coinbase.vin[0].scriptSig = CScript() << CScriptNum::serialize(4711)
                                        << OP_0;
  block.vtx[0] = MakeTransactionRef(coinbase);

  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::NO_SNAPSHOT_HASH), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(check_scriptsig_with_additional_data) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  coinbase.vin[0].scriptSig = CScript() << CScriptNum::serialize(4711)
                                        << ToByteVector(uint256())
                                        << ToByteVector(uint256());
  block.vtx[0] = MakeTransactionRef(coinbase);

  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result.Is(Error::NO_BLOCK_HEIGHT));
  BOOST_CHECK(!validation_result.Is(Error::NO_SNAPSHOT_HASH));
}

BOOST_AUTO_TEST_CASE(check_NO_snapshot_hash) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  coinbase.vin[0].scriptSig = CScript() << CScriptNum::serialize(7) << OP_0;
  block.vtx[0] = MakeTransactionRef(coinbase);

  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::NO_SNAPSHOT_HASH), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(check_empty_coinbase_transaction) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  // empty coinbase transaction
  auto coinbase = CMutableTransaction();
  coinbase.SetType(TxType::COINBASE);
  block.vtx[0] = MakeTransactionRef(coinbase);

  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::NO_META_INPUT), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(check_coinbase_transaction_without_stake) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  // remove coin stake input
  coinbase.vin.erase(coinbase.vin.begin() + 1);
  block.vtx[0] = MakeTransactionRef(coinbase);

  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::NO_STAKING_INPUT), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(no_public_key) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinbase = CMutableTransaction(*block.vtx[0]);
  // remove public key from staking input's witness stack
  coinbase.vin[1].scriptWitness.stack.clear();
  block.vtx[0] = MakeTransactionRef(coinbase);
  block.hash_witness_merkle_root = BlockWitnessMerkleRoot(block);
  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::INVALID_BLOCK_PUBLIC_KEY), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(invalid_block_signature) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  // corrupt signature by flipping some byte
  block.signature[7] = ~block.signature[7];
  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::BLOCK_SIGNATURE_VERIFICATION_FAILED), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(invalid_block_time) {
  const auto block_validator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  // corrupt block time by offsetting it by 1
  block.nTime = block.nTime + 1;
  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(block, nullptr);

  BOOST_CHECK(!validation_result);
  BOOST_CHECK_MESSAGE(validation_result.Is(Error::INVALID_BLOCK_TIME), validation_result.GetRejectionMessage());
}

BOOST_AUTO_TEST_CASE(valid_block) {
  staking::BlockValidationInfo block_validation_info;

  const auto block_validator = staking::BlockValidator::New(b.get());
  const staking::BlockValidationResult validation_result = block_validator->CheckBlock(MinimalBlock(), &block_validation_info);

  BOOST_CHECK_MESSAGE(validation_result, validation_result.GetRejectionMessage());
  BOOST_CHECK(static_cast<bool>(block_validation_info.GetCheckBlockStatus()));

  const blockchain::Height expected_height = 4711;
  const uint256 expected_snapshot_hash =
      uint256S("689dae90b6913ff34a64750dd537177afa58b3d012803a10793d74f1ebb88da9");

  BOOST_CHECK_EQUAL(validation_result.GetRejectionMessage(), "");
  BOOST_CHECK_EQUAL(block_validation_info.GetSnapshotHash(), expected_snapshot_hash);
  BOOST_CHECK_EQUAL(block_validation_info.GetHeight(), expected_height);
}

BOOST_AUTO_TEST_CASE(check_mismatching_height) {

  const auto block_validator = staking::BlockValidator::New(b.get());

  CBlockIndex prev_block;
  prev_block.nHeight = 1499;

  {
    staking::BlockValidationInfo block_validation_info;
    block_validation_info.MarkCheckBlockHeaderSuccessfull();
    block_validation_info.MarkContextualCheckBlockHeaderSuccessfull();
    block_validation_info.MarkCheckBlockSuccessfull(1500, uint256());

    const staking::BlockValidationResult validation_result =
        block_validator->ContextualCheckBlock(MinimalBlock(), prev_block, std::time(nullptr), &block_validation_info);
    BOOST_CHECK_MESSAGE(validation_result, validation_result.GetRejectionMessage());
    BOOST_CHECK(block_validation_info.GetContextualCheckBlockStatus().IsTrue());
  }

  {
    staking::BlockValidationInfo block_validation_info;
    block_validation_info.MarkCheckBlockHeaderSuccessfull();
    block_validation_info.MarkContextualCheckBlockHeaderSuccessfull();
    block_validation_info.MarkCheckBlockSuccessfull(1500, uint256());

    prev_block.nHeight = 1498;

    const staking::BlockValidationResult validation_result =
        block_validator->ContextualCheckBlock(MinimalBlock(), prev_block, std::time(nullptr), &block_validation_info);
    BOOST_CHECK(!validation_result);
    BOOST_CHECK(validation_result.Is(Error::MISMATCHING_HEIGHT));
    BOOST_CHECK(block_validation_info.GetContextualCheckBlockStatus().IsFalse());
  }
}

BOOST_AUTO_TEST_CASE(genesis_block_testnet) {
  CheckGenesisBlock(blockchain::Parameters::TestNet());
}

BOOST_AUTO_TEST_CASE(genesis_block_regtest) {
  CheckGenesisBlock(blockchain::Parameters::RegTest());
}

BOOST_AUTO_TEST_SUITE_END()
