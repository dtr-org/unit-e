// Copyright (c) 2018 The Unit-e developers
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
  auto pair = m_curDynastyVotes.emplace(epoch, 0);
  return pair.first->second;
}
uint64_t Checkpoint::GetPrevDynastyVotes(uint32_t epoch) {
  auto pair = m_prevDynastyVotes.emplace(epoch, 0);
  return pair.first->second;
}

bool Checkpoint::operator==(const Checkpoint &other) const {
  return m_isJustified == other.m_isJustified &&
         m_isFinalized == other.m_isFinalized &&
         m_curDynastyDeposits == other.m_curDynastyDeposits &&
         m_prevDynastyDeposits == other.m_prevDynastyDeposits &&
         m_curDynastyVotes == other.m_curDynastyVotes &&
         m_prevDynastyVotes == other.m_prevDynastyVotes &&
         m_voteSet == other.m_voteSet;
}

}  // namespace esperanza
