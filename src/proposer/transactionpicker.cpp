// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/transactionpicker.h>

#include <amount.h>
#include <miner.h>
#include <script/script.h>

namespace proposer {

//! An adapter to bitcoins CBlockAssembler.
//!
//! The CBlockAssembler comprises the logic for picking transactions.
//! In order to maintain compatibility with bitcoin but not rely on
//! CBlockTemplate and not change existing code this adapter is used
//! to just extract the transactions to be included when building a new
//! block.
//!
//! CBlockTemplate is an invention to support external mining software.
//! Previous iterations of bitcoin had an rpc method called "getwork"
//! which would only return a block header to solve the hash for. This
//! effectively took away power from the miners in a mining pool and
//! centralize the decision which transactions to include in mined blocks
//! with the pool operator. To combat this BIP22 and BIP23 defined the
//! "getblocktemplate" rpc to supersede "getwork".
//!
//! Since there is no mining in unit-e we do not use the block
//! templates. The proposer can assemble a block itself, which in turn
//! greatly reduces complexity of the process to create new blocks and
//! the amount of code needed to do so.
//!
//! The implementation, in fact the existence, of this very class is
//! hidden from other compilation units since declaration and definition
//! are here in the *.cpp-file. This is on purpose and inspired by the
//! pImpl idiom. It should decrease compile times and helps
//! encapsulation. Since TransactionPicker is an interface which
//! can be mocked easily this design should greatly help unit testing
//! components which use a TransactionPicker.
class BlockAssemblerAdapter final : public TransactionPicker {

 private:
  const CChainParams &m_chainParams;

 public:
  explicit BlockAssemblerAdapter(const CChainParams &chainParams)
      : m_chainParams(chainParams) {}

  ~BlockAssemblerAdapter() override = default;

  PickTransactionsResult PickTransactions(
      const PickTransactionsParameters &parameters) override {

    ::BlockAssembler::Options blockAssemblerOptions;
    blockAssemblerOptions.blockMinFeeRate = parameters.m_minFees;
    blockAssemblerOptions.nBlockMaxWeight = parameters.m_maxWeight;

    ::BlockAssembler blockAssembler(m_chainParams, blockAssemblerOptions);

    // The block assembler unfortunately also creates a bitcoin-style
    // coinbase transaction. We do not want to touch that logic to
    // retain compatibility with bitcoin. The construction of the
    // coinstake transaction is left to the component using a
    // TransactionPicker to build a block. Therefore we just pass an
    // empty script to the blockAssembler.
    CScript script(1);
    script.push_back(OP_RETURN);
    std::unique_ptr<CBlockTemplate> blockTemplate =
        blockAssembler.CreateNewBlock(script, /* fMineWitnessTx */ true);

    PickTransactionsResult result{std::move(blockTemplate->block.vtx),
                                  std::move(blockTemplate->vTxFees)};

    return result;
  };
};

std::unique_ptr<TransactionPicker> TransactionPicker::BlockAssemblerAdapter(
    const CChainParams &chainParams) {

  return std::unique_ptr<TransactionPicker>(
      new class BlockAssemblerAdapter(chainParams));
}

}  // namespace proposer
