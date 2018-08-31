// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNIT_E_VALIDATION_H
#define UNIT_E_VALIDATION_H

#include <primitives/block.h>

namespace esperanza {

bool HasIsCoinstakeOp(const CScript &scriptIn);

bool GetCoinstakeScriptPath(const CScript &scriptIn, CScript &scriptOut);

bool CheckStakeUnused(const COutPoint &kernel);

bool CheckStakeUnique(const CBlock &block, bool update);

int GetNumBlocksOfPeers();

} // namespace esperanza

#endif //UNIT_E_VALIDATION_H
