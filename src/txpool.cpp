// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <net_processing.h>
#include <txmempool.h>
#include <txpool.h>
#include <validation.h>

class TxPoolEnumeratorImpl : public TxPool {
 public:
  size_t GetTxCount() const override {
    LOCK2(g_cs_orphans, mempool.cs);

    // We don't use vExtraTxnForCompact here because it is a cyclic buffer and
    // it causes several issues:
    // - Hard to count its content - you really need to iterate it
    // - It's content is only removed when new "cycle" comes. So it can contain
    //   all kinds of outdated txs, including those that are already in blocks
    return mempool.size() + mapOrphanTransactions.size();
  }

  std::vector<CTransactionRef> GetTxs() const override {
    LOCK2(g_cs_orphans, mempool.cs);

    std::vector<CTransactionRef> result;
    result.reserve(GetTxCount());

    for (const auto &entry : mempool.mapTx) {
      result.emplace_back(entry.GetSharedTx());
    }

    for (const auto &entry : mapOrphanTransactions) {
      result.emplace_back(entry.second.tx);
    }

    return result;
  }
};

std::unique_ptr<TxPool> TxPool::New() {
  return MakeUnique<TxPoolEnumeratorImpl>();
}
