// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/vote.h>
#include <key.h>
#include <primitives/transaction.h>

#ifndef UNITE_TEST_ESPERANZA_FINALIZATION_UTILS_H
#define UNITE_TEST_ESPERANZA_FINALIZATION_UTILS_H

CTransaction CreateVoteTx(const esperanza::Vote &vote, const CKey &spendable_key);

CTransaction CreateVoteTx(const CTransaction &spendable_tx, const CKey &spendable_key,
                          const esperanza::Vote &vote, const std::vector<unsigned char> &vote_sig);

CTransaction CreateDepositTx(const CTransaction &spendable_tx,
                             const CKey &spendable_key,
                             const CAmount &amount,
                             const CAmount &change = 0);

CTransaction CreateLogoutTx(const CTransaction &spendable_tx,
                            const CKey &spendable_key, CAmount amount);

CTransaction CreateWithdrawTx(const CTransaction &spendable_tx,
                              const CKey &spendable_key, CAmount amount);

CTransaction CreateP2PKHTx(const CTransaction &spendable_tx,
                           const CKey &spendable_key, CAmount amount);

#endif  // UNITE_TEST_ESPERANZA_FINALIZATION_UTILS_H
