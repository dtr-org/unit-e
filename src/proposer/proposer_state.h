// Copyright (c) 2018-2019 The Unit-e developers
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

  //! \brief how many times the proposer looked for an eligible coin
  std::uint64_t m_number_of_searches = 0;

  //! \brief how many times the proposer found an eligible coin
  std::uint64_t m_number_of_search_attempts = 0;

  //! \brief how many blocks the proposer actually proposed
  std::uint64_t m_number_of_proposed_blocks = 0;

  //! \brief how many transactions the proposer included in proposed blocks in total
  std::uint64_t m_number_of_transactions_included = 0;
};

}  // namespace proposer

#endif  // UNIT_E_PROPOSER_STATE_H
