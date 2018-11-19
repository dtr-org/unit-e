// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_PROPOSER_STATE_H
#define UNIT_E_PROPOSER_PROPOSER_STATE_H

#include <proposer/proposer_status.h>

namespace proposer {

//! bookkeeping data per wallet
struct State {

  Status m_status = Status::NOT_PROPOSING;

  int64_t m_lastCoinStakeSearchTime = 0;

  //! when did this proposer propose most recently
  int64_t m_lastTimeProposed = 0;

  //! how many search cycles the proposer went through
  uint64_t m_numSearches = 0;

  //! how many search cycles the proposer attempted
  uint64_t m_numSearchAttempts = 0;
};

}  // namespace proposer

#endif  // UNIT_E_PROPOSER_STATE_H
