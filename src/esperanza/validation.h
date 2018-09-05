// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_VALIDATION_H
#define UNITE_ESPERANZA_VALIDATION_H

#include <consensus/validation.h>
#include <chain.h>
#include <primitives/block.h>

namespace esperanza {

bool CheckDepositTransaction(CValidationState &state, const CTransaction &tx,
                             const CBlockIndex *pindex = nullptr);

bool CheckVoteTransaction(CValidationState &state, const CTransaction &tx,
                          const CBlockIndex *pindex = nullptr);

} // namespace esperanza

#endif //UNIT_E_VALIDATION_H
