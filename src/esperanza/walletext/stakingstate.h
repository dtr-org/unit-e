// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNITE_ESPERANZA_WALLETEXT_STAKINGSTATE_H
#define UNITE_ESPERANZA_WALLETEXT_STAKINGSTATE_H

#include <cstdint>

namespace esperanza {

namespace walletext {

enum class StakingState : uint32_t {
  NOT_STAKING,
  IS_STAKING,
  NOT_STAKING_BALANCE,
  NOT_STAKING_DEPTH,
  NOT_STAKING_LOCKED,
  NOT_STAKING_LIMITED,
};

} // namespace walletext

} // namespace esperanza

#endif // UNITE_ESPERANZA_WALLETEXT_STAKINGSTATE_H
