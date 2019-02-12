// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_LTOR_H
#define UNITE_LTOR_H

#include <vector>
#include <primitives/transaction.h>

namespace ltor {

void SortTransactionsWithLTOR(std::vector<CTransactionRef> &transactions);

}

#endif //UNITE_LTOR_H
