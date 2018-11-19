// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_TRANSACTIONPICKER_H
#define UNIT_E_PROPOSER_TRANSACTIONPICKER_H

#include <vector>

#include <amount.h>
#include <chainparams.h>
#include <dependency.h>
#include <policy/policy.h>
#include <primitives/transaction.h>

namespace proposer {

//! \brief a component for picking transactions for a new block.
//!
//! When building a new block to be proposed the proposer has to fill
//! that block with transactions. The TransactionPicker is the component
//! which selects the transactions.
//!
//! This class is an interface.
//!
//! Currently the only implementation of the TransactionPicker is an
//! adapter to bitcoins CBlockAssembler. A conceivable alternative
//! implementation would take into account maybe a minimum transaction
//! amount (but that might also have been taken care of by transaction
//! relay policies – then again a proposer might still very well include
//! his own micro transaction which would have to be tackled by a
//! consensus rule anyway and therefore would be reflected in a
//! TransactionPicker).
class TransactionPicker {

 public:
  struct PickTransactionsParameters {
    //! \brief The maximum weight of the block to pick transactions for.
    //!
    //! BIP141 introduced a new method for computing the max block
    //! size which is the block weight. The block weight is defined
    //! as base-size * 3 + total_size. According to BIP141 the block
    //! weight must be less-than-or-equal-to 4M.
    unsigned int m_maxWeight = DEFAULT_BLOCK_MAX_WEIGHT;

    //! \brief The minimum sum of transaction fees
    //!
    //! The incentive to include transactions into a block is to
    //! harvest the transaction fees. Fees are set when a transaction
    //! is created. The fees are the difference of the inputs being
    //! spent and the outputs created.
    unsigned int m_minFees = DEFAULT_BLOCK_MIN_TX_FEE;
  };

  //! \brief transactions and fees chosen.
  struct PickTransactionsResult {
    std::vector<CTransactionRef> m_transactions;
    std::vector<CAmount> m_fees;
  };

  //! \brief picks transactions for inclusion in a block
  //!
  //! Chooses transactions to be included into a newly proposed
  //! block, according to the parameters passed in.
  virtual PickTransactionsResult PickTransactions(
      const PickTransactionsParameters &) = 0;

  virtual ~TransactionPicker() = default;

  //! \brief Factory method for creating a BlockAssemblerAdapter
  static std::unique_ptr<TransactionPicker> MakeBlockAssemblerAdapter();
};

}  // namespace proposer

#endif  // UNIT_E_PROPOSER_TRANSACTIONPICKER_H
