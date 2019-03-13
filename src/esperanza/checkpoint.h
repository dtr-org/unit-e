// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_CHECKPOINT_H
#define UNITE_ESPERANZA_CHECKPOINT_H

#include <amount.h>
#include <uint256.h>
#include <map>
#include <set>
#include <vector>

namespace esperanza {

class Checkpoint {
 public:
  Checkpoint();

  bool m_is_justified;
  bool m_is_finalized;

  uint64_t m_cur_dynasty_deposits;
  uint64_t m_prev_dynasty_deposits;

  std::map<uint32_t, uint64_t> m_cur_dynasty_votes;
  std::map<uint32_t, uint64_t> m_prev_dynasty_votes;

  // Set of validatorAddresses for validators that voted that checkpoint
  std::set<uint160> m_vote_set;

  uint64_t GetCurDynastyVotes(uint32_t epoch);
  uint64_t GetPrevDynastyVotes(uint32_t epoch);

  bool operator==(const Checkpoint &other) const;
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_CHECKPOINT_H
