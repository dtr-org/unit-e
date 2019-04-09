// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_TXPOOL_H
#define UNITE_TXPOOL_H

#include <primitives/transaction.h>

//! \brief Interface that wraps access to both mempool and orphanpool
class TxPool {
 public:
  virtual size_t GetTxCount() const = 0;
  virtual std::vector<CTransactionRef> GetTxs() const = 0;
  virtual ~TxPool() = default;

  static std::unique_ptr<TxPool> New();
};

#endif  //UNITE_TXPOOL_H
