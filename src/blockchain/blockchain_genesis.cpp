// Copyright (c) 2018 The The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_genesis.h>

#include <arith_uint256.h>
#include <consensus/merkle.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>
#include <util.h>
#include <utilstrencodings.h>
#include <utiltime.h>

namespace blockchain {

class GenesisBlockBuilderImpl : public GenesisBlockBuilder {

 private:
  uint32_t m_version = 4;
  uint32_t m_time = 0;
  uint32_t m_bits = 0x1d00ffff;
  std::vector<std::pair<CAmount, CTxDestination>> m_initial_funds;

  const CTransactionRef BuildCoinstakeTransaction() const {
    CMutableTransaction tx;

    tx.SetVersion(2);
    tx.SetType(TxType::COINSTAKE);

    const CScript scriptSig = CScript() << CScriptNum::serialize(0)  // height
                                        << ToByteVector(uint256())   // utxo set hash
                                        << OP_0;

    tx.vin.emplace_back(uint256(), 0, scriptSig);

    for (const auto &target : m_initial_funds) {
      const CAmount amount = target.first;
      const CTxDestination destination = target.second;
      const CScript scriptPubKey = GetScriptForDestination(destination);
      tx.vout.emplace_back(amount, scriptPubKey);
    }

    return MakeTransactionRef(tx);
  }

 public:
  const CBlock Build() const override {
    CBlock genesis_block;

    genesis_block.nVersion = m_version;
    genesis_block.nTime = m_time;
    genesis_block.nBits = m_bits;

    CTransactionRef coinstake_transaction = BuildCoinstakeTransaction();
    genesis_block.vtx.push_back(coinstake_transaction);

    genesis_block.hashPrevBlock = uint256();
    genesis_block.hashMerkleRoot = BlockMerkleRoot(genesis_block);

    // UNIT-E: TODO: This will be enabled once we merge the proposer/segwit pull request
    // genesis_block.hashWitnessMerkleRoot = BlockWitnessMerkleRoot(genesis_block);

    assert(genesis_block.vtx.size() == 1);
    assert(genesis_block.vtx[0]->vin.size() == 1);
    assert(genesis_block.vtx[0]->vin[0].prevout.hash == uint256());
    assert(genesis_block.vtx[0]->vin[0].prevout.n == 0);
    assert(genesis_block.vtx[0]->vout.size() == m_initial_funds.size());
    assert(genesis_block.hashMerkleRoot == genesis_block.vtx[0]->GetHash());

    // UNIT-E: TODO: This will be enabled once we merge the proposer/segwit pull request
    // assert(genesis_block.hashWitnessMerkleRoot == genesis_block.hashMerkleRoot);

    return genesis_block;
  }

  void SetVersion(const uint32_t version) override {
    m_version = version;
  }

  void SetTime(const uint32_t time) override {
    m_time = time;
  }

  void SetBits(const uint32_t bits) override {
    m_bits = bits;
  }

  void SetDifficulty(const uint256 difficulty) override {
    m_bits = UintToArith256(difficulty).GetCompact();
  }

  void AddFundsForPayToPubKeyHash(const CAmount amount,
                                  const std::string &hexKey) override {
    const std::vector<std::uint8_t> data = ParseHex(hexKey);
    const uint160 pubKeyHash(data);
    m_initial_funds.emplace_back(amount, WitnessV0KeyHash(pubKeyHash));
  }

  void AddFundsForPayToScriptHash(const CAmount amount,
                                  const std::string &hexScriptHash) override {
    const std::vector<std::uint8_t> data = ParseHex(hexScriptHash);
    const uint256 scriptHash(data);
    m_initial_funds.emplace_back(amount, WitnessV0ScriptHash(scriptHash));
  }
};

std::unique_ptr<GenesisBlockBuilder> GenesisBlockBuilder::New() {
  return MakeUnique<GenesisBlockBuilderImpl>();
}

P2WPKH::P2WPKH(const CAmount amount, const std::string &&pubKeyHash)
    : amount(amount), pubKeyHash(pubKeyHash) {
  assert(amount > 0);
  assert(pubKeyHash.size() == 40);
}

P2WSH::P2WSH(const CAmount amount, const std::string &&scriptHash)
    : amount(amount), scriptHash(scriptHash) {
  assert(amount > 0);
  assert(scriptHash.size() == 64);
}

GenesisBlock::GenesisBlock(const CBlock &block)
    : block(block), hash(block.GetHash()) {}

namespace {

const CBlock Build(std::initializer_list<P2WPKH> outputs) {
  std::unique_ptr<GenesisBlockBuilder> b = GenesisBlockBuilder::New();

  for (const auto &output : outputs) {
    b->AddFundsForPayToPubKeyHash(output.amount, output.pubKeyHash);
  }

  return b->Build();
}

}  // namespace

GenesisBlock::GenesisBlock(std::initializer_list<P2WPKH> outputs)
    : GenesisBlock(Build(outputs)) {}

}  // namespace blockchain
