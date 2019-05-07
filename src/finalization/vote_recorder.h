// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_FINALIZATION_VOTE_RECORDER_H
#define UNIT_E_FINALIZATION_VOTE_RECORDER_H

#include <dbwrapper.h>
#include <esperanza/vote.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <sync.h>

#include <boost/optional.hpp>

#include <map>
#include <memory>

class CValidationState;

namespace esperanza {
class FinalizationState;
}

namespace finalization {

using FinalizationState = esperanza::FinalizationState;

struct VoteRecord {
  esperanza::Vote vote;
  std::vector<unsigned char> sig;

  bool operator==(const VoteRecord &b) const {
    return vote == b.vote && sig == b.sig;
  }

  CScript GetScript() const;

  ADD_SERIALIZE_METHODS

  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(vote);
    READWRITE(sig);
  }
};

class VoteRecorder : private boost::noncopyable {
 public:
  struct DBParams {
    size_t cache_size = 0;
    bool inmemory = false;
    bool wipe = false;
    bool obfuscate = false;
  };

 private:
  VoteRecorder(const DBParams &p);

  // Contains a map by validatorAddress. Each entry contains a map of the target
  // epoch height with the actual vote
  std::map<uint160, std::map<uint32_t, VoteRecord>> voteRecords;

  // Contains the most recent vote casted by any validator
  std::map<uint160, VoteRecord> voteCache;

  CDBWrapper m_db;

  static CCriticalSection cs_recorder;
  static std::shared_ptr<VoteRecorder> g_voteRecorder;

  boost::optional<VoteRecord> FindOffendingVote(const esperanza::Vote &vote);
  void LoadFromDB();
  void SaveVoteToDB(const VoteRecord &record);

 public:
  void RecordVote(const esperanza::Vote &vote,
                  const std::vector<unsigned char> &voteSig,
                  const FinalizationState &fin_state,
                  bool log_errors = true);

  boost::optional<VoteRecord> GetVote(const uint160 &validatorAddress,
                                      uint32_t epoch) const;

  static void Init(const DBParams &params);
  static void Reset(const DBParams &params);
  static std::shared_ptr<VoteRecorder> GetVoteRecorder();
};

//! \brief Records the vote.
//!
//! tx must be a vote transaction
//! fin_state is a FinalizationState VoteRecorder must rely on when it checks transaction validity
//!           and slashable condition. It must be best known finalization state on a moment.
bool RecordVote(const CTransaction &tx,
                CValidationState &err_state,
                const FinalizationState &fin_state,
                bool log_errors = true);

}  // namespace finalization

#endif  // UNIT_E_FINALIZATION_VOTE_RECORDER_H
