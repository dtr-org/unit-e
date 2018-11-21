#ifndef UNIT_E_FINALIZATION_VOTE_RECORDER_H
#define UNIT_E_FINALIZATION_VOTE_RECORDER_H

#include <boost/optional.hpp>
#include <esperanza/finalizationstate.h>
#include <esperanza/vote.h>
#include <map>
#include <memory>
#include <primitives/transaction.h>

namespace finalization {

struct VoteRecord {
  esperanza::Vote vote;
  std::vector<unsigned char> sig;

  CScript GetScript() const;
};

class VoteRecorder {
private:
  // Contains a map by validatorId. Each entry contains a map of the target
  // epoch height with the actual vote
  std::map<uint160, std::map<uint32_t, VoteRecord>> voteRecords;

  // Contains the most recent vote casted by any validator
  std::map<uint160, VoteRecord> voteCache;

  static CCriticalSection cs_recorder;
  static std::shared_ptr<VoteRecorder> g_voteRecorder;

  boost::optional<VoteRecord> FindOffendingVote(esperanza::Vote vote);

public:
  void RecordVote(const esperanza::Vote &vote,
                  const std::vector<unsigned char> voteSig);

  boost::optional<VoteRecord> GetVote(const uint160 validatorAddress,
                                      uint32_t epoch) const;

  static void Init();
  static void Reset();
  static std::shared_ptr<VoteRecorder> GetVoteRecorder();
};

} // namespace finalization

#endif // UNIT_E_FINALIZATION_VOTE_RECORDER_H
