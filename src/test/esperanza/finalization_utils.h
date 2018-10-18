//
// Created by Matteo Sumberaz on 17.10.18.
//

#include <esperanza/vote.h>
#include <primitives/transaction.h>

#ifndef UNIT_E_ESPERANZA_FINALIZATION_UTILS_H
#define UNIT_E_ESPERANZA_FINALIZATION_UTILS_H

CTransaction CreateVoteTx(esperanza::Vote &vote);

CTransaction CreateDepositTx(const CTransaction &spendableTx,
                             const CKey &spendableKey, CAmount amount);

CTransaction CreateLogoutTx(const CTransaction &spendableTx,
                            const CKey &spendableKey, CAmount amount);

CTransaction CreateWithdrawTx(const CTransaction &spendableTx,
                              const CKey &spendableKey, CAmount amount);

CTransaction CreateP2PKHTx(const CTransaction &spendableTx,
                           const CKey &spendableKey, CAmount amount);

#endif  // UNIT_E_ESPERANZA_FINALIZATION_UTILS_H
