// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/blockproposer.h>

#include <consensus/merkle.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/script.h>
#include <wallet/wallet.h>

namespace proposer {

class BlockProposerImpl : public BlockProposer {

 private:
  Dependency<ChainState> m_chain;
  Dependency<TransactionPicker> m_transactionPicker;

  //! \brief builds the meta txin (zeroth txin in a coinstake tx)
  //!
  //! The meta txin is the zeroth input of a coinstake transaction. It does
  //! not refer to any UTXO but contains meta data:
  //!
  //! - the height of the block
  //! - the utxo set hash (to support snapshot/fast sync)
  //!
  //! The meta txin is accompanied by a witness program that contains the
  //! public key that is used to sign the block.
  CTxIn BuildMetaTxIn(const uint32_t blockHeight, const uint256 &utxoSetHash,
                      const CPubKey &pubKey) const {

    std::vector<uint8_t> serializedUTXOSetHash(utxoSetHash.begin(),
                                               utxoSetHash.end());

    CTxIn txIn;
    txIn.prevout.SetNull();
    txIn.scriptSig = CScript() << static_cast<int64_t>(blockHeight)
                               << serializedUTXOSetHash << 0x00;
    txIn.scriptWitness.stack.emplace_back(pubKey.begin(), pubKey.end());

    return txIn;
  }

 public:
  explicit BlockProposerImpl(Dependency<ChainState> chain,
                             Dependency<TransactionPicker> transactionPicker)
      : m_chain(chain), m_transactionPicker(transactionPicker) {}

  CTransaction BuildCoinstakeTransaction(
      const CoinstakeParameters &parameters) const override {

    CMutableTransaction mutableTx;
    mutableTx.SetType(TxType::COINSTAKE);
    mutableTx.SetVersion(1);

    // create meta input
    mutableTx.vin.emplace_back(BuildMetaTxIn(
        parameters.blockHeight, parameters.utxoSetHash, parameters.pubKey));

    // create stake inputs
    for (const auto &coin : parameters.stake) {
      const int ix = coin->nIndex;
      mutableTx.vin.emplace_back(coin->tx->GetHash(), ix);
    }

    // sign inputs
    unsigned int txInIndex = 1;
    for (const auto &coin : parameters.stake) {
      parameters.wallet->SignInput(coin, mutableTx, txInIndex);
      ++txInIndex;
    }

    return CTransaction(mutableTx);
  }

  std::shared_ptr<const CBlock> ProposeBlock(
      const ProposeBlockParameters &parameters) override {

    CoinstakeParameters coinstakeParameters;
    coinstakeParameters.blockHeight = parameters.blockHeight;
    coinstakeParameters.wallet = parameters.wallet;

    const CTransaction coinstakeTransaction =
        BuildCoinstakeTransaction(coinstakeParameters);

    TransactionPicker::PickTransactionsParameters pickTransactionsParameters;

    TransactionPicker::PickTransactionsResult transactionsResult =
        m_transactionPicker->PickTransactions(pickTransactionsParameters);

    CBlock block;

    block.nTime = static_cast<uint32_t>(parameters.blockTime);
    block.vtx = transactionsResult.m_transactions;

    bool isValid = false;
    block.hashMerkleRoot = ::BlockMerkleRoot(block, &isValid);

    if (!isValid) {
      return nullptr;
    }

    auto sharedBlock = std::make_shared<const CBlock>(block);

    if (!m_chain->ProcessNewBlock(sharedBlock)) {
      return nullptr;
    }
    return sharedBlock;
  }
};

std::unique_ptr<BlockProposer> BlockProposer::MakeBlockProposer(
    Dependency<ChainState> chain,
    Dependency<TransactionPicker> transactionPicker) {
  return std::unique_ptr<BlockProposer>(
      new BlockProposerImpl(chain, transactionPicker));
}

}  // namespace proposer
