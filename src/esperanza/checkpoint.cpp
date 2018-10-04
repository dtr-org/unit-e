// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/checkpoint.h>

namespace esperanza {

Checkpoint::Checkpoint()
    : m_isJustified(false),
      m_isFinalized(false),
      m_curDynastyDeposits(0),
      m_prevDynastyDeposits(0),
      m_voteMap() {}

}  // namespace esperanza
