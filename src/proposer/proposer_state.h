// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_PROPOSER_STATE_H
#define UNIT_E_PROPOSER_PROPOSER_STATE_H

#include <proposer/proposer_status.h>

#include <cstdint>

namespace proposer {

//! bookkeeping data per wallet
struct State {

  Status m_status = Status::NOT_PROPOSING;

  //! how many search cycles the proposer went through
  std::uint64_t m_number_of_searches = 0;

  //! how many search cycles the proposer attempted
  std::uint64_t m_number_of_search_attempts = 0;
};

}  // namespace proposer

#endif  // UNIT_E_PROPOSER_STATE_H
