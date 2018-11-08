#include <finalization/vote_recorder.h>
#include <wallet/wallet.h>

namespace finalization {

CCriticalSection VoteRecorder::cs_recorder;
std::shared_ptr<VoteRecorder> VoteRecorder::g_voteRecorder;

VoteRecorder::VoteRecorder(esperanza::WalletExtension &wallet)
    : wallet(wallet) {}

void VoteRecorder::RecordVote(const CTransaction &transaction,
                              const esperanza::Vote &vote) {

  LOCK(cs_recorder);
  assert(wallet.validatorState);

  esperanza::FinalizationState *state =
      esperanza::FinalizationState::GetState();

  // Check if the vote comes from a validator
  if (!state->GetValidator(vote.m_validatorIndex)) {
    return;
  }

  // We don't need to hold a lock here since this should not change after
  // startup.
  if (!wallet.nIsValidatorEnabled) {
    return;
  }

  if (wallet.validatorState.get().m_validatorIndex == vote.m_validatorIndex) {
    // Assume that votes casted by the node itself are valid.
    return;
  }

  boost::optional<esperanza::Vote> offendingVote = FindOffendingVote(vote);
  if (offendingVote) {
    esperanza::Result res = state->IsSlashable(vote, offendingVote.get());
    if (res == +esperanza::Result::SUCCESS) {
      CWalletTx outTx;
      wallet.SendSlash(transaction, vote, offendingVote.get(), outTx);
    } else {
      //If this happens then it needs urgent attention and fixing
      LogPrint(BCLog::FINALIZATION,
               "ERROR: The offending vote found is not valid: %s, cannot "
               "reliably identify slashable votes.",
               res._to_string());
      assert(false);
    }
  }
}

boost::optional<esperanza::Vote> VoteRecorder::FindOffendingVote(const esperanza::Vote vote) {

  auto cacheIt = voteCache.find(vote.m_validatorIndex);
  if (cacheIt != voteCache.end()) {
    if (cacheIt->second == vote) {
      // Result was cached
      return boost::none;
    }
  }

  esperanza::Vote slashCandidate;
  auto it = voteRecords.find(vote.m_validatorIndex);
  if (it != voteRecords.end()) {

    auto voteMap = it->second;

    // Check for double votes
    auto vit = voteMap.find(vote.m_targetEpoch);
    if (vit != voteMap.end()) {
      if (vit->second.m_targetHash != vote.m_targetHash) {
        return vit->second;
      }
    } else {
      voteMap.emplace(vote.m_targetEpoch, vote);
    }

    // Check for a surrounding vote
    vit = voteMap.lower_bound(vote.m_sourceEpoch);
    while (vit != voteMap.begin()) {
      if (vit->second.m_sourceEpoch < vote.m_targetEpoch) {
        if ((vit->second.m_sourceEpoch > vote.m_sourceEpoch &&
             vit->second.m_targetEpoch < vote.m_targetEpoch) ||
            (vit->second.m_sourceEpoch < vote.m_sourceEpoch &&
             vit->second.m_targetEpoch > vote.m_targetEpoch)) {
          return vit->second;
        }
      }
      ++vit;
    }

  } else {
    std::map<uint32_t, esperanza::Vote> newMap;
    newMap.emplace(vote.m_targetEpoch, vote);
    voteRecords.emplace(vote.m_validatorIndex, newMap);
  }

  return boost::none;
}

void VoteRecorder::InitVoteRecorder(esperanza::WalletExtension &wallet) {
  LOCK(cs_recorder);
  if (!g_voteRecorder) {
    g_voteRecorder = std::shared_ptr<VoteRecorder>(new VoteRecorder(wallet));
  }
}

std::shared_ptr<VoteRecorder> VoteRecorder::GetVoteRecorder() {
  return g_voteRecorder;
}
}  // namespace finalization
