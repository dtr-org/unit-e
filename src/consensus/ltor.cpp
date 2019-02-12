// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <algorithm>
#include <consensus/ltor.h>

namespace ltor {

void SortTransactionsWithLTOR(std::vector<CTransactionRef> &transactions) {
    // LTOR/CTOR: We ensure that all transactions (except the 0th, coinbase) are
    // sorted in lexicographical order. Notice that we don't sort
    // blocktemplate->vTxFees nor blocktemplate->vTxSigOpsCost because they are
    // not used at all. These two vectors are just a "residue" from Bitcoin's
    // PoW mining pools code.
    std::sort(
        std::begin(transactions) + 1, std::end(transactions),
        [](const CTransactionRef &a, const CTransactionRef &b) -> bool {
          return a->GetHash().CompareAsNumber(b->GetHash()) < 0;
        }
    );
}

}