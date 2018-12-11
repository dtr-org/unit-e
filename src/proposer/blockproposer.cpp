// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/blockproposer.h>

#include <consensus/merkle.h>

namespace proposer {

class BlockProposerImpl : public BlockProposer {

 private:
  Dependency<staking::ActiveChain> m_chain;
  Dependency<staking::TransactionPicker> m_transactionPicker;

 public:
  explicit BlockProposerImpl(Dependency<staking::ActiveChain> chain,
                             Dependency<staking::TransactionPicker> transactionPicker)
      : m_chain(chain), m_transactionPicker(transactionPicker) {}

  std::shared_ptr<const CBlock> ProposeBlock(
      const ProposeBlockParameters &parameters) override {

    staking::TransactionPicker::PickTransactionsParameters pickTransactionsParameters{};

    staking::TransactionPicker::PickTransactionsResult transactionsResult =
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

std::unique_ptr<BlockProposer> BlockProposer::New(
    Dependency<staking::ActiveChain> chain,
    Dependency<staking::TransactionPicker> transactionPicker) {
  return std::unique_ptr<BlockProposer>(
      new BlockProposerImpl(chain, transactionPicker));
}

}  // namespace proposer
