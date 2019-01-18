// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_genesis.h>

#include <arith_uint256.h>
#include <blockchain/blockchain_behavior.h>
#include <blockchain/blockchain_types.h>
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

const CTransactionRef GenesisBlockBuilder::BuildCoinbaseTransaction() const {
  CMutableTransaction tx;

  tx.SetVersion(2);
  tx.SetType(TxType::COINBASE);

  const CScript scriptSig = CScript() << CScriptNum::serialize(0)  // height
                                      << ToByteVector(uint256());  // utxo set hash

  tx.vin.emplace_back(uint256(), 0, scriptSig);

  for (const auto &target : m_initial_funds) {
    const CAmount amount = target.first;
    const CTxDestination &destination = target.second;
    const CScript scriptPubKey = GetScriptForDestination(destination);
    tx.vout.emplace_back(amount, scriptPubKey);
  }

  return MakeTransactionRef(tx);
}

const CBlock GenesisBlockBuilder::Build(const Parameters &parameters) const {

  auto behavior = Behavior::NewFromParameters(parameters);

  CBlock genesis_block;

  genesis_block.nVersion = m_version;
  genesis_block.nTime = behavior->CalculateProposingTimestamp(m_time);
  genesis_block.nBits = m_bits;

  CTransactionRef coinbase_transaction = BuildCoinbaseTransaction();
  genesis_block.vtx.push_back(coinbase_transaction);

  genesis_block.hashPrevBlock = uint256();
  genesis_block.hashMerkleRoot = BlockMerkleRoot(genesis_block);

  // explicitly set signature to null (there's no stake and no public key which could sign)
  genesis_block.signature.clear();

  // UNIT-E: TODO: This will be enabled once we merge the proposer/segwit pull request
  // genesis_block.hashWitnessMerkleRoot = BlockWitnessMerkleRoot(genesis_block);

  assert(genesis_block.vtx.size() == 1);
  assert(genesis_block.vtx[0]->vin.size() == 1);
  assert(genesis_block.vtx[0]->vin[0].prevout.hash == uint256());
  assert(genesis_block.vtx[0]->vin[0].prevout.n == 0);
  assert(genesis_block.vtx[0]->vout.size() == m_initial_funds.size());

  // UNIT-E: TODO: This will be enabled once we will have defined the initial funds allocation
  //  CAmount initial_funds_amount = 0;
  //  for(const auto& out : m_initial_funds) {
  //    initial_funds_amount += out.first;
  //  }
  //  assert(initial_funds_amount == parameters.initial_supply);

  assert(genesis_block.hashMerkleRoot == genesis_block.vtx[0]->GetHash());

  // UNIT-E: TODO: This will be enabled once we merge the proposer/segwit pull request
  // assert(genesis_block.hashWitnessMerkleRoot == genesis_block.hashMerkleRoot);

  return genesis_block;
}

GenesisBlockBuilder &GenesisBlockBuilder::SetVersion(const uint32_t version) {
  m_version = version;
  return *this;
}

GenesisBlockBuilder &GenesisBlockBuilder::SetTime(const blockchain::Time time) {
  m_time = time;
  return *this;
}

GenesisBlockBuilder &GenesisBlockBuilder::SetBits(const blockchain::Difficulty bits) {
  m_bits = bits;
  return *this;
}

GenesisBlockBuilder &GenesisBlockBuilder::SetDifficulty(const uint256 difficulty) {
  m_bits = UintToArith256(difficulty).GetCompact();
  return *this;
}

GenesisBlockBuilder &GenesisBlockBuilder::AddFundsForPayToPubKeyHash(const CAmount amount,
                                                                     const std::string &hexKey) {
  const std::vector<std::uint8_t> data = ParseHex(hexKey);
  const uint160 pubKeyHash(data);
  m_initial_funds.emplace_back(amount, WitnessV0KeyHash(pubKeyHash));
  return *this;
}

GenesisBlockBuilder &GenesisBlockBuilder::AddFundsForPayToScriptHash(const CAmount amount,
                                                                     const std::string &hexScriptHash) {
  const std::vector<std::uint8_t> data = ParseHex(hexScriptHash);
  const uint256 scriptHash(data);
  m_initial_funds.emplace_back(amount, WitnessV0ScriptHash(scriptHash));
  return *this;
}

GenesisBlockBuilder &GenesisBlockBuilder::Add(const Funds &&funds) {
  for (const auto &output : funds.destinations) {
    AddFundsForPayToPubKeyHash(output.amount, output.pub_key_hash);
  }
  return *this;
}

P2WPKH::P2WPKH(const CAmount amount, const std::string &&pubKeyHash)
    : amount(amount), pub_key_hash(pubKeyHash) {
  assert(amount > 0);
  assert(pubKeyHash.size() == 40);
}

P2WSH::P2WSH(const CAmount amount, const std::string &&scriptHash)
    : amount(amount), script_hash(scriptHash) {
  assert(amount > 0);
  assert(scriptHash.size() == 64);
}

Funds::Funds(std::initializer_list<P2WPKH> ds)
    : destinations(ds) {}

}  // namespace blockchain
