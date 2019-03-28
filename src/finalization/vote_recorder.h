// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_FINALIZATION_VOTE_RECORDER_H
#define UNIT_E_FINALIZATION_VOTE_RECORDER_H

#include <esperanza/vote.h>
#include <primitives/transaction.h>
#include <sync.h>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <map>
#include <memory>

class CValidationState;

namespace esperanza {
class FinalizationState;
}

namespace finalization {

struct VoteRecord {
  esperanza::Vote vote;
  std::vector<unsigned char> sig;

  bool operator==(const VoteRecord &b) const {
    return vote == b.vote && sig == b.sig;
  }

  CScript GetScript() const;
};

class VoteRecorder : private boost::noncopyable {
 private:
  VoteRecorder() = default;

  // Contains a map by validatorAddress. Each entry contains a map of the target
  // epoch height with the actual vote
  std::map<uint160, std::map<uint32_t, VoteRecord>> voteRecords;

  // Contains the most recent vote casted by any validator
  std::map<uint160, VoteRecord> voteCache;

  static CCriticalSection cs_recorder;
  static std::shared_ptr<VoteRecorder> g_voteRecorder;

  boost::optional<VoteRecord> FindOffendingVote(const esperanza::Vote &vote);

 public:
  void RecordVote(const esperanza::Vote &vote,
                  const std::vector<unsigned char> &voteSig);

  boost::optional<VoteRecord> GetVote(const uint160 &validatorAddress,
                                      uint32_t epoch) const;

  static void Init();
  static void Reset();
  static std::shared_ptr<VoteRecorder> GetVoteRecorder();
};

bool RecordVote(const CTransaction &tx, CValidationState &err_state);

}  // namespace finalization

#endif  // UNIT_E_FINALIZATION_VOTE_RECORDER_H
