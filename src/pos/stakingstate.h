// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNIT_E_STAKINGSTATE_H
#define UNIT_E_STAKINGSTATE_H

#include <stdint.h>

enum class StakingState : int8_t
{
    NOT_STAKING             = 0,
    IS_STAKING              = 1,
    NOT_STAKING_BALANCE     = -1,
    NOT_STAKING_DEPTH       = -2,
    NOT_STAKING_LOCKED      = -3,
    NOT_STAKING_LIMITED     = -4,
};

#endif //UNIT_E_STAKINGSTATE_H
