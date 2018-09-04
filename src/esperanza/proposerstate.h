// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNIT_E_ESPERANZA_PROPOSERSTATE_H
#define UNIT_E_ESPERANZA_PROPOSERSTATE_H

#include <stdint.h>

namespace esperanza {

enum class ProposerState : uint32_t {
  NOT_PROPOSING,
  IS_PROPOSING,
  NOT_PROPOSING_NOT_ENOUGH_BALANCE,
  NOT_STAKING_DEPTH,
  NOT_PROPOSING_WALLET_LOCKED,
  NOT_STAKING_LIMITED,
};

}

#endif // UNIT_E_ESPERANZA_PROPOSERSTATE_H
