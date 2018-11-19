#include <finalization/vote_recorder.h>
#include <util.h>
#include <validationinterface.h>

namespace finalization {

CCriticalSection VoteRecorder::cs_recorder;
std::shared_ptr<VoteRecorder> VoteRecorder::g_voteRecorder;

void VoteRecorder::RecordVote(const esperanza::Vote &vote,
                              const std::vector<unsigned char> voteSig) {

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
  auto it = voteRecords.find(vote.m_validatorAddress);
  if (it != voteRecords.end()) {
    auto inIt = it->second.find(vote.m_targetEpoch);
    if (inIt == it->second.end()) {
      it->second.emplace(vote.m_targetEpoch, voteRecord);
    }
  } else {
    std::map<uint32_t, VoteRecord> newMap;
    newMap.emplace(vote.m_targetEpoch, voteRecord);
    voteRecords.emplace(vote.m_validatorAddress, newMap);
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
               "reliably identify slashable votes.",
               res._to_string());
      assert(false);
    }
  }
}

boost::optional<VoteRecord>
VoteRecorder::FindOffendingVote(const esperanza::Vote vote) {

  auto cacheIt = voteCache.find(vote.m_validatorAddress);
  if (cacheIt != voteCache.end()) {
    if (cacheIt->second.vote == vote) {
      // Result was cached
      return boost::none;
    }
  }

  esperanza::Vote slashCandidate;
  auto it = voteRecords.find(vote.m_validatorAddress);
  if (it != voteRecords.end()) {

    auto voteMap = it->second;

    // Check for double votes
    auto vit = voteMap.find(vote.m_targetEpoch);
    if (vit != voteMap.end()) {
      if (vit->second.vote.m_targetHash != vote.m_targetHash) {
        return vit->second;
      }
    }

    // Check for a surrounding vote
    vit = voteMap.lower_bound(vote.m_sourceEpoch);
    while (vit != voteMap.end()) {
      if (vit->second.vote.m_sourceEpoch < vote.m_targetEpoch) {
        if ((vit->second.vote.m_sourceEpoch > vote.m_sourceEpoch &&
             vit->second.vote.m_targetEpoch < vote.m_targetEpoch) ||
            (vit->second.vote.m_sourceEpoch < vote.m_sourceEpoch &&
             vit->second.vote.m_targetEpoch > vote.m_targetEpoch)) {
          return vit->second;
        }
      }
      ++vit;
    }
  }

  return boost::none;
}

boost::optional<VoteRecord>
VoteRecorder::GetVote(const uint160 validatorAddress, uint32_t epoch) const {

  auto it = voteRecords.find(validatorAddress);
  if (it != voteRecords.end()) {
    auto it2 = it->second.find(epoch);
    if (it2 != it->second.end()) {
      return it2->second;
    }
    return boost::none;
  }

  return boost::none;
}

void VoteRecorder::Init() {
  LOCK(cs_recorder);
  if (!g_voteRecorder) {
    g_voteRecorder = std::shared_ptr<VoteRecorder>(new VoteRecorder());
  }
}

void VoteRecorder::Reset() {
  LOCK(cs_recorder);
  g_voteRecorder = std::shared_ptr<VoteRecorder>(new VoteRecorder());
}

std::shared_ptr<VoteRecorder> VoteRecorder::GetVoteRecorder() {
  return g_voteRecorder;
}

CScript VoteRecord::GetScript() const { return CScript::EncodeVote(vote, sig); }
} // namespace finalization
