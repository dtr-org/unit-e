#include <finalization/vote_recorder.h>
#include <util.h>
#include <validationinterface.h>

namespace finalization {

CCriticalSection VoteRecorder::cs_recorder;
std::shared_ptr<VoteRecorder> VoteRecorder::g_voteRecorder;

void VoteRecorder::RecordVote(const esperanza::Vote &vote,
                              const std::vector<unsigned char> &voteSig) {

  LOCK(cs_recorder);

  esperanza::FinalizationState *state =
      esperanza::FinalizationState::GetState();

  // Check if the vote comes from a validator
  if (!state->GetValidator(vote.m_validatorAddress)) {
    return;
  }

  boost::optional<VoteRecord> offendingVote = FindOffendingVote(vote);

  VoteRecord voteRecord{vote, voteSig};

  // Record the vote
  auto validatorIt = voteRecords.find(vote.m_validatorAddress);
  if (validatorIt != voteRecords.end()) {
    auto recordIt = validatorIt->second.find(vote.m_targetEpoch);
    validatorIt->second.emplace(vote.m_targetEpoch, voteRecord);
  } else {
    std::map<uint32_t, VoteRecord> newMap;
    newMap.emplace(vote.m_targetEpoch, voteRecord);
    voteRecords.emplace(vote.m_validatorAddress, std::move(newMap));
  }

  if (offendingVote) {
    esperanza::Result res = state->IsSlashable(vote, offendingVote.get().vote);
    if (res == +esperanza::Result::SUCCESS) {
      GetMainSignals().SlashingConditionDetected(VoteRecord{vote, voteSig},
                                                 offendingVote.get());
      LogPrint(BCLog::FINALIZATION,
               "%s: Slashable event found. Sending signal to the wallet.",
               __func__);
    } else {
      // If this happens then it needs urgent attention and fixing
      LogPrint(BCLog::FINALIZATION,
               "ERROR: The offending vote found is not valid: %s, cannot "
               "reliably identify slashable votes. Please fix.",
               res._to_string());
      assert(false);
    }
  }
}

boost::optional<VoteRecord> VoteRecorder::FindOffendingVote(const esperanza::Vote &vote) {

  auto cacheIt = voteCache.find(vote.m_validatorAddress);
  if (cacheIt != voteCache.end()) {
    if (cacheIt->second.vote == vote) {
      // Result was cached
      return boost::none;
    }
  }

  esperanza::Vote slashCandidate;
  auto validatorIt = voteRecords.find(vote.m_validatorAddress);
  if (validatorIt != voteRecords.end()) {

    const auto &voteMap = validatorIt->second;

    // Check for double votes
    auto recordIt = voteMap.find(vote.m_targetEpoch);
    if (recordIt != voteMap.end()) {
      if (recordIt->second.vote.m_targetHash != vote.m_targetHash) {
        return recordIt->second;
      }
    }

    // Check for a surrounding vote
    recordIt = voteMap.lower_bound(vote.m_sourceEpoch);
    while (recordIt != voteMap.end()) {
      const VoteRecord &record = recordIt->second;
      if (record.vote.m_sourceEpoch < vote.m_targetEpoch) {
        if ((record.vote.m_sourceEpoch > vote.m_sourceEpoch &&
             record.vote.m_targetEpoch < vote.m_targetEpoch) ||
            (record.vote.m_sourceEpoch < vote.m_sourceEpoch &&
             record.vote.m_targetEpoch > vote.m_targetEpoch)) {
          return record;
        }
      }
      ++recordIt;
    }
  }
  return boost::none;
}

boost::optional<VoteRecord> VoteRecorder::GetVote(const uint160 validatorAddress, uint32_t epoch) const {

  auto validatorIt = voteRecords.find(validatorAddress);
  if (validatorIt != voteRecords.end()) {
    auto recordIt = validatorIt->second.find(epoch);
    if (recordIt != validatorIt->second.end()) {
      return recordIt->second;
    }
  }
  return boost::none;
}

void VoteRecorder::Init() {
  LOCK(cs_recorder);
  if (!g_voteRecorder) {
    g_voteRecorder = std::make_shared<VoteRecorder>(new VoteRecorder());
  }
}

void VoteRecorder::Reset() {
  LOCK(cs_recorder);
  g_voteRecorder = std::make_shared<VoteRecorder>(new VoteRecorder());
}

std::shared_ptr<VoteRecorder> VoteRecorder::GetVoteRecorder() {
  return g_voteRecorder;
}

CScript VoteRecord::GetScript() const { return CScript::EncodeVote(vote, sig); }
}  // namespace finalization
