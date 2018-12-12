// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_parameters.h>
#include <consensus/merkle.h>
#include <key/mnemonic/mnemonic.h>
#include <staking/block_validator.h>
#include <test/test_unite.h>

#include <boost/test/unit_test.hpp>

namespace {

std::unique_ptr<blockchain::Behavior> b =
    blockchain::Behavior::FromParameters(blockchain::Parameters::MainNet());

//! \brief creates a minimal block that passes validation without looking at the chain
CBlock MinimalBlock() {
  // a block is signed by the proposer, thus we need some key setup here
  const key::mnemonic::Seed seed("cook note face vicious suggest company unit smart lobster tongue dune diamond faculty solid thought");
  const CExtKey &extKey = seed.GetExtKey();
  // public key for signing block
  const CPubKey pubKey = extKey.Neuter().pubkey;
  const auto pubKeyData = std::vector<unsigned char>(pubKey.begin(), pubKey.end());

  CBlock block;
  block.nTime = b->CalculateProposingTimestamp(std::time(nullptr));
  {
    CMutableTransaction tx;
    tx.SetType(TxType::COINSTAKE);
    // meta input: block height, snapshot hash, terminator
    CScript scriptSig = CScript() << CScriptNum::serialize(4711)
                                  << ToByteVector(uint256());
    tx.vin.emplace_back(uint256(), 0, scriptSig);
    // stake
    tx.vin.emplace_back(uint256(), 1);
    tx.vin[1].scriptWitness.stack.emplace_back();  // signature, not checked
    tx.vin[1].scriptWitness.stack.emplace_back(pubKeyData);
    // can be spent by anyone, simply yields "true"
    CScript scriptPubKey = CScript() << OP_TRUE;
    tx.vout.emplace_back(50, scriptPubKey);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }
  {
    CMutableTransaction tx;
    tx.SetType(TxType::STANDARD);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }

  bool duplicateTransactions;
  block.hashMerkleRoot = BlockMerkleRoot(block, &duplicateTransactions);
  const uint256 blockHash = block.GetHash();

  extKey.key.Sign(blockHash, block.signature);

  return block;
}

}  // namespace

BOOST_FIXTURE_TEST_SUITE(block_validator_tests, BasicTestingSetup)

using Error = staking::BlockValidationError;

BOOST_AUTO_TEST_CASE(check_empty_block) {
  const auto blockValidator = staking::BlockValidator::New(b.get());

  CBlock block;

  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(!validationResult);
  BOOST_CHECK(validationResult.Contains(Error::NO_TRANSACTIONS));
}

BOOST_AUTO_TEST_CASE(check_first_transaction_not_a_coinstake_transaction) {
  const auto blockValidator = staking::BlockValidator::New(b.get());

  CMutableTransaction tx;
  tx.SetType(TxType::STANDARD);

  CBlock block;
  block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));

  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(!validationResult);
  BOOST_CHECK(validationResult.Contains(Error::FIRST_TRANSACTION_NOT_A_COINSTAKE_TRANSACTION));
  BOOST_CHECK(!validationResult.Contains(Error::COINSTAKE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST));
}

BOOST_AUTO_TEST_CASE(check_coinstake_other_than_first) {
  const auto blockValidator = staking::BlockValidator::New(b.get());

  CBlock block;
  {
    CMutableTransaction tx;
    tx.SetType(TxType::STANDARD);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }
  {
    CMutableTransaction tx;
    tx.SetType(TxType::COINSTAKE);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }

  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(!validationResult);
  BOOST_CHECK(validationResult.Contains(Error::COINSTAKE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST));
  BOOST_CHECK(validationResult.Contains(Error::FIRST_TRANSACTION_NOT_A_COINSTAKE_TRANSACTION));
}

BOOST_AUTO_TEST_CASE(check_two_coinstake_transactions) {
  const auto blockValidator = staking::BlockValidator::New(b.get());

  CBlock block;
  {
    CMutableTransaction tx;
    tx.SetType(TxType::COINSTAKE);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }
  {
    CMutableTransaction tx;
    tx.SetType(TxType::COINSTAKE);
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }

  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(validationResult.Contains(Error::COINSTAKE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST));
  BOOST_CHECK(!validationResult.Contains(Error::FIRST_TRANSACTION_NOT_A_COINSTAKE_TRANSACTION));
  BOOST_CHECK(!validationResult);
}

BOOST_AUTO_TEST_CASE(check_NO_block_height) {
  const auto blockValidator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinstake = CMutableTransaction(*block.vtx[0]);
  coinstake.vin[0].scriptSig = CScript() << OP_0;
  block.vtx[0] = MakeTransactionRef(coinstake);

  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(validationResult.Contains(Error::NO_BLOCK_HEIGHT));
  BOOST_CHECK(validationResult.Contains(Error::NO_SNAPSHOT_HASH));
  BOOST_CHECK(!validationResult);
}

BOOST_AUTO_TEST_CASE(check_premature_end_of_scriptsig) {
  const auto blockValidator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinstake = CMutableTransaction(*block.vtx[0]);
  coinstake.vin[0].scriptSig = CScript() << CScriptNum::serialize(4711)
                                         << OP_0;
  block.vtx[0] = MakeTransactionRef(coinstake);

  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(!validationResult.Contains(Error::NO_BLOCK_HEIGHT));
  BOOST_CHECK(validationResult.Contains(Error::NO_SNAPSHOT_HASH));
  BOOST_CHECK(!validationResult);
}

BOOST_AUTO_TEST_CASE(check_scriptsig_with_additional_data) {
  const auto blockValidator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinstake = CMutableTransaction(*block.vtx[0]);
  coinstake.vin[0].scriptSig = CScript() << CScriptNum::serialize(4711)
                                         << ToByteVector(uint256())
                                         << ToByteVector(uint256());
  block.vtx[0] = MakeTransactionRef(coinstake);

  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(!validationResult.Contains(Error::NO_BLOCK_HEIGHT));
  BOOST_CHECK(!validationResult.Contains(Error::NO_SNAPSHOT_HASH));
}

BOOST_AUTO_TEST_CASE(check_NO_snapshot_hash) {
  const auto blockValidator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinstake = CMutableTransaction(*block.vtx[0]);
  coinstake.vin[0].scriptSig = CScript() << CScriptNum::serialize(7) << OP_0;
  block.vtx[0] = MakeTransactionRef(coinstake);

  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(!validationResult.Contains(Error::NO_BLOCK_HEIGHT));
  BOOST_CHECK(validationResult.Contains(Error::NO_SNAPSHOT_HASH));
  BOOST_CHECK(!validationResult);
}

BOOST_AUTO_TEST_CASE(check_empty_coinstake_transaction) {
  const auto blockValidator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  // empty coinstake transaction
  auto coinstake = CMutableTransaction();
  coinstake.SetType(TxType::COINSTAKE);
  block.vtx[0] = MakeTransactionRef(coinstake);

  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(validationResult.Contains(Error::NO_META_INPUT));
  BOOST_CHECK(validationResult.Contains(Error::COINSTAKE_TRANSACTION_WITHOUT_OUTPUT));
  BOOST_CHECK(validationResult.Contains(Error::NO_STAKING_INPUT));
  BOOST_CHECK(!validationResult);
}

BOOST_AUTO_TEST_CASE(check_coinstake_transaction_without_stake) {
  const auto blockValidator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinstake = CMutableTransaction(*block.vtx[0]);
  // remove coin stake input
  coinstake.vin.erase(coinstake.vin.begin() + 1);
  block.vtx[0] = MakeTransactionRef(coinstake);

  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(validationResult.Contains(Error::NO_STAKING_INPUT));
  BOOST_CHECK(!validationResult);
}

BOOST_AUTO_TEST_CASE(no_public_key) {
  const auto blockValidator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  auto coinstake = CMutableTransaction(*block.vtx[0]);
  // remove public key from staking input's witness stack
  coinstake.vin[1].scriptWitness.stack.clear();
  block.vtx[0] = MakeTransactionRef(coinstake);
  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(validationResult.Contains(Error::NO_BLOCK_PUBLIC_KEY));
  BOOST_CHECK(!validationResult);
}

BOOST_AUTO_TEST_CASE(invalid_block_signature) {
  const auto blockValidator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  // corrupt signature by flipping some byte
  block.signature[7] = ~block.signature[7];
  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(!validationResult);
  BOOST_CHECK(validationResult.Contains(Error::BLOCK_SIGNATURE_VERIFICATION_FAILED));
}

BOOST_AUTO_TEST_CASE(invalid_block_time) {
  const auto blockValidator = staking::BlockValidator::New(b.get());
  CBlock block = MinimalBlock();
  // corrupt block time by offsetting it by 1
  block.nTime = block.nTime + 1;
  const auto validationResult = blockValidator->CheckBlock(block);

  BOOST_CHECK(!validationResult);
  BOOST_CHECK(validationResult.Contains(Error::INVALID_BLOCK_TIME));
}

BOOST_AUTO_TEST_CASE(valid_block) {
  const auto blockValidator = staking::BlockValidator::New(b.get());
  const auto validationResult = blockValidator->CheckBlock(MinimalBlock());

  BOOST_CHECK((bool)validationResult);
}

BOOST_AUTO_TEST_SUITE_END()
