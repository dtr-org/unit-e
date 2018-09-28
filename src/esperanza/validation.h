// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_VALIDATION_H
#define UNITE_ESPERANZA_VALIDATION_H

#include <chain.h>
#include <consensus/validation.h>
#include <primitives/block.h>

namespace esperanza {

bool IsVoteExpired(const CTransaction &tx);

bool CheckDepositTransaction(CValidationState &errState, const CTransaction &tx,
                             const CBlockIndex *pindex = nullptr);

bool CheckVoteTransaction(CValidationState &errState, const CTransaction &tx,
                          const CBlockIndex *pindex = nullptr);

bool CheckLogoutTransaction(CValidationState &errState, const CTransaction &tx,
                            const CBlockIndex *pindex = nullptr);

bool CheckWithdrawTransaction(CValidationState &errState,
                              const CTransaction &tx,
                              const CBlockIndex *pindex = nullptr);

}  // namespace esperanza

#endif  // UNIT_E_VALIDATION_H
