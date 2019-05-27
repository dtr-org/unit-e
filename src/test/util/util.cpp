// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/util.h>

#include <consensus/ltor.h>
#include <consensus/merkle.h>
#include <key/mnemonic/mnemonic.h>

#include <boost/test/unit_test.hpp>

KeyFixture MakeKeyFixture(const std::string &seed_words) {
  // a block is signed by the proposer, thus we need some key setup here
  const key::mnemonic::Seed seed(seed_words);
  const CExtKey &ext_key = seed.GetExtKey();
  // public key for signing block
  const CPubKey pub_key = ext_key.key.GetPubKey();
  return {
      blockchain::Behavior::NewFromParameters(blockchain::Parameters::TestNet()),
      ext_key,
      pub_key,
      std::vector<unsigned char>(pub_key.begin(), pub_key.end())};
}

CTransactionRef MakeCoinbaseTransaction(const KeyFixture &key_fixture, const blockchain::Height height) {

  CMutableTransaction tx;
  tx.SetType(TxType::COINBASE);

  // meta input: block height, snapshot hash, terminator
  CScript script_sig = CScript() << CScriptNum::serialize(4711)
                                 << ToByteVector(uint256S("689dae90b6913ff34a64750dd537177afa58b3d012803a10793d74f1ebb88da9"));
  tx.vin.emplace_back(COutPoint(), script_sig);
  // stake
  tx.vin.emplace_back(uint256::zero, 1);
  tx.vin[1].scriptWitness.stack.emplace_back();  // signature, not checked
  tx.vin[1].scriptWitness.stack.emplace_back(key_fixture.pub_key_data);
  // can be spent by anyone, simply yields "true"
  CScript script_pub_key = CScript() << OP_TRUE;
  tx.vout.emplace_back(50, script_pub_key);
  return MakeTransactionRef(CTransaction(tx));
}

CBlock MinimalBlock(const KeyFixture &key_fixture) {
  return MinimalBlock([](CBlock& block) {}, key_fixture);
}

CBlock MinimalBlock(const std::function<void(CBlock &)> block_augmentor,
                    const KeyFixture &key_fixture) {
  CBlock block;
  block.nTime = key_fixture.blockchain_behavior->CalculateProposingTimestamp(std::time(nullptr));
  block.vtx.emplace_back(MakeCoinbaseTransaction(key_fixture));
  {
    CMutableTransaction tx;
    tx.SetType(TxType::REGULAR);
    tx.vin.emplace_back(uint256::zero, 3);
    tx.vout.emplace_back(8, CScript());
    block.vtx.push_back(MakeTransactionRef(CTransaction(tx)));
  }
  block_augmentor(block);
  ltor::SortTransactions(block.vtx);
  block.hashMerkleRoot = BlockMerkleRoot(block);
  block.hash_witness_merkle_root = BlockWitnessMerkleRoot(block);
  block.hash_finalizer_commits_merkle_root = BlockFinalizerCommitsMerkleRoot(block);
  const uint256 blockHash = block.GetHash();
  key_fixture.ext_key.key.Sign(blockHash, block.signature);
  return block;
}

