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
      m_voteSet() {}

uint64_t Checkpoint::GetCurDynastyVotes(uint32_t epoch) {
  auto it = m_curDynastyVotes.find(epoch);
  if (it == m_curDynastyVotes.end()) {
    m_curDynastyVotes[epoch] = 0;
    return 0;
  }
  return it->second;
}
uint64_t Checkpoint::GetPrevDynastyVotes(uint32_t epoch) {
  auto it = m_prevDynastyVotes.find(epoch);
  if (it == m_prevDynastyVotes.end()) {
    m_prevDynastyVotes[epoch] = 0;
    return 0;
  }
  return it->second;
}
}  // namespace esperanza
