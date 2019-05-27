// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/checkpoint.h>
#include <util/system.h>

namespace esperanza {

Checkpoint::Checkpoint()
    : m_is_justified(false),
      m_is_finalized(false),
      m_cur_dynasty_deposits(0),
      m_prev_dynasty_deposits(0),
      m_vote_set() {}

uint64_t Checkpoint::GetCurDynastyVotes(uint32_t epoch) {
  auto pair = m_cur_dynasty_votes.emplace(epoch, 0);
  return pair.first->second;
}
uint64_t Checkpoint::GetPrevDynastyVotes(uint32_t epoch) {
  auto pair = m_prev_dynasty_votes.emplace(epoch, 0);
  return pair.first->second;
}

bool Checkpoint::operator==(const Checkpoint &other) const {
  return m_is_justified == other.m_is_justified &&
         m_is_finalized == other.m_is_finalized &&
         m_cur_dynasty_deposits == other.m_cur_dynasty_deposits &&
         m_prev_dynasty_deposits == other.m_prev_dynasty_deposits &&
         m_cur_dynasty_votes == other.m_cur_dynasty_votes &&
         m_prev_dynasty_votes == other.m_prev_dynasty_votes &&
         m_vote_set == other.m_vote_set;
}

std::string Checkpoint::ToString() const {
  return strprintf(
      "Checkpoint{"
      "m_is_justified=% "
      "m_is_finalized=%d "
      "m_cur_dynasty_deposits=%d "
      "m_prev_dynasty_deposits=%d "
      "m_cur_dynasty_votes=%s "
      "m_prev_dynasty_votes=%s "
      "m_vote_set=%s}",
      m_is_justified,
      m_is_finalized,
      m_cur_dynasty_deposits,
      m_prev_dynasty_deposits,
      util::to_string(m_cur_dynasty_votes),
      util::to_string(m_prev_dynasty_votes),
      util::to_string(m_vote_set));
}

}  // namespace esperanza
