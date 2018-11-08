#ifndef UNIT_E_FINALIZATION_VOTE_RECORDER_H
#define UNIT_E_FINALIZATION_VOTE_RECORDER_H

#include <esperanza/finalizationstate.h>
#include <esperanza/vote.h>
#include <esperanza/walletextension.h>
#include <primitives/transaction.h>
#include <boost/optional.hpp>
#include <map>
#include <memory>

namespace finalization {

// We have to add to the cache when a new vote is add. We can remove from the
// cache when 1. there is slashing (or we see someone being slashed), 2.
// when the validator has withdrawn.
class VoteRecorder {
 private:
  // Contains a map by validatorId. Each entry contains a map of the target
  // epoch height with the actual vote
  std::map<uint256, std::map<uint32_t, esperanza::Vote>> voteRecords;

  // Contains the most recent vote casted by any validator
  std::map<uint256, esperanza::Vote> voteCache;

  esperanza::WalletExtension &wallet;

  static CCriticalSection cs_recorder;
  static std::shared_ptr<VoteRecorder> g_voteRecorder;

  boost::optional<esperanza::Vote> FindOffendingVote(esperanza::Vote vote);

 public:
  explicit VoteRecorder(esperanza::WalletExtension &wallet);

  void RecordVote(const CTransaction &transaction, const esperanza::Vote &vote);

  static void InitVoteRecorder(esperanza::WalletExtension &wallet);
  static std::shared_ptr<VoteRecorder> GetVoteRecorder();
};

}  // namespace finalization

#endif  // UNIT_E_FINALIZATION_VOTE_RECORDER_H
