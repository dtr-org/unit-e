// Copyright (c) 2018 The unit-e core developers
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
  std::vector<std::pair<CAmount, CKeyID>> m_initial_funds;

  const CTransactionRef BuildCoinstakeTransaction() const {
    CMutableTransaction tx;

    tx.SetVersion(2);
    tx.SetType(TxType::COINSTAKE);

    const std::string comment("Whereof one cannot speak, thereof one must be silent.");
    const CScript scriptSig = CScript() << std::vector<uint8_t>(comment.cbegin(), comment.cend());

    tx.vin.emplace_back(uint256(), 0, scriptSig);

    for (const auto &target : m_initial_funds) {
      const CAmount amount = target.first;
      const CKeyID keyID = target.second;

      const CTxDestination destination = WitnessV0KeyHash(keyID);
      const CScript scriptPubKey = GetScriptForDestination(destination);
      tx.vout.emplace_back(amount, scriptPubKey);
    }

    return MakeTransactionRef(tx);
  }

 public:
  const GenesisBlock Build() const override {
    GenesisBlock genesis_block_wrapper;
    CBlock &genesis_block = genesis_block_wrapper.block;

    genesis_block.nVersion = m_version;
    genesis_block.nTime = m_time;
    genesis_block.nBits = m_bits;

    CTransactionRef coinstake_transaction = BuildCoinstakeTransaction();
    genesis_block.vtx.push_back(coinstake_transaction);

    genesis_block.hashPrevBlock = uint256();
    genesis_block.hashMerkleRoot = BlockMerkleRoot(genesis_block);
    genesis_block.hashWitnessMerkleRoot = BlockWitnessMerkleRoot(genesis_block);

    assert(genesis_block.vtx.size() == 1);
    assert(genesis_block.vtx[0]->vin.size() == 1);
    assert(genesis_block.vtx[0]->vin[0].prevout.hash == uint256());
    assert(genesis_block.vtx[0]->vin[0].prevout.n == 0);
    assert(genesis_block.vtx[0]->vout.size() == m_initial_funds.size());
    assert(genesis_block.hashMerkleRoot == genesis_block.vtx[0]->GetHash());
    assert(genesis_block.hashWitnessMerkleRoot == uint256());

    genesis_block_wrapper.hash = genesis_block.GetHash();

    return genesis_block_wrapper;
  };

  void SetVersion(const uint32_t version) override {
    m_version = version;
  };

  void SetTime(const uint32_t time) override {
    m_time = time;
  };

  void SetBits(const uint32_t bits) override {
    m_bits = bits;
  };

  void SetDifficulty(const uint256 difficulty) override {
    m_bits = UintToArith256(difficulty).GetCompact();
  };

#ifdef SOME_CONSTANT_THAT_IS_SURELY_NOT_DEFINED
  void AddFundsForWallet(const CAmount amount,
                         const std::string &mnemonic,
                         const std::string &passphrase) override {
    CWallet wallet;
    key::mnemonic::Seed seed(mnemonic, passphrase);
    const CPubKey masterKey = wallet.GenerateNewHDMasterKey(&seed);
    wallet.SetHDMasterKey(masterKey);
    CPubKey pubKey;
    if (!wallet.GetKeyFromPool(pubKey)) {
      return;
    }
    const CKeyID keyID = pubKey.GetID();
    const std::string keyHash = HexStr(keyID.begin(), keyID.end());
    AddFundsForPubKeyHash(amount, keyHash);
  };
#endif

  void AddFundsForPubKey(const CAmount amount,
                         const std::string &hexKey) override {
    const std::vector<std::uint8_t> data = ParseHex(hexKey);
    const CPubKey pubKey(data.cbegin(), data.cend());
    m_initial_funds.emplace_back(amount, pubKey.GetID());
  }

  void AddFundsForPubKeyHash(const CAmount amount,
                             const std::string &hexKey) override {
    const std::vector<std::uint8_t> data = ParseHex(hexKey);
    uint160 keyID(data);
    m_initial_funds.emplace_back(amount, keyID);
  };
};

std::unique_ptr<GenesisBlockBuilder> GenesisBlockBuilder::New() {
  return MakeUnique<GenesisBlockBuilderImpl>();
}

GenesisOutput::GenesisOutput(const CAmount amount, const std::string &&pubKeyHash)
    : amount(amount), pubKeyHash(std::move(pubKeyHash)) {
  assert(amount > 0);
  assert(pubKeyHash.size() == 40);
}

GenesisBlock::GenesisBlock(std::initializer_list<GenesisOutput> outputs) {
  std::unique_ptr<GenesisBlockBuilder> b = GenesisBlockBuilder::New();

  for (const auto &output : outputs) {
    b->AddFundsForPubKeyHash(output.amount, output.pubKeyHash);
  }

  *this = b->Build();
}

}  // namespace blockchain
