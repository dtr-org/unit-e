// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/transactionpicker.h>

#include <amount.h>
#include <miner.h>
#include <script/script.h>

namespace staking {

//! \brief An adapter to bitcoins BlockAssembler.
//!
//! The BlockAssembler comprises the logic for picking transactions.
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
class BlockAssemblerAdapter final : public TransactionPicker {

 public:
  ~BlockAssemblerAdapter() override = default;

  PickTransactionsResult PickTransactions(
      const PickTransactionsParameters &parameters) override {

    BlockAssembler::Options block_assembler_options;
    block_assembler_options.blockMinFeeRate = parameters.min_fees;
    block_assembler_options.nBlockMaxWeight = parameters.max_weight;

    BlockAssembler block_assembler(::Params(), block_assembler_options);

    PickTransactionsResult result;
    try {
      std::unique_ptr<CBlockTemplate> block_template =
          block_assembler.PickTransactions();

      result.transactions.swap(block_template->block.vtx);
        result.fees.swap(block_template->vTxFees);
      }
    } catch (const std::runtime_error &err) {
      result.error = err.what();
    }
    return result;
  };
};

std::unique_ptr<TransactionPicker>
TransactionPicker::New() {
  return MakeUnique<BlockAssemblerAdapter>();
}

}  // namespace staking
