// Copyright (c) 2018 The unit-e core developers
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

  bool m_isJustified;
  bool m_isFinalized;

  uint64_t m_curDynastyDeposits;
  uint64_t m_prevDynastyDeposits;

  std::map<uint32_t, uint64_t> m_curDynastyVotes;
  std::map<uint32_t, uint64_t> m_prevDynastyVotes;

  // Set of validatorIndexes for validators that voted that checkpoint
  std::set<uint256> m_voteSet;

  uint64_t GetCurDynastyVotes(uint32_t epoch);
  uint64_t GetPrevDynastyVotes(uint32_t epoch);
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_CHECKPOINT_H
