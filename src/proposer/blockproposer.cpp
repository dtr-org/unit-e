// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/blockproposer.h>

#include <consensus/merkle.h>

namespace proposer {

class BlockProposerImpl : public BlockProposer {

 private:
  Dependency<ChainState> m_chain;
  Dependency<TransactionPicker> m_transactionPicker;

 public:
  explicit BlockProposerImpl(Dependency<ChainState> chain,
                             Dependency<TransactionPicker> transactionPicker)
      : m_chain(chain), m_transactionPicker(transactionPicker) {}

  std::shared_ptr<const CBlock> ProposeBlock(
      const ProposeBlockParameters &parameters) override {

    TransactionPicker::PickTransactionsParameters pickTransactionsParameters{};

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
