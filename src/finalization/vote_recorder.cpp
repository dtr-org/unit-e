// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/vote_recorder.h>

#include <consensus/validation.h>
#include <esperanza/finalizationstate.h>
#include <injector.h>
#include <util.h>
#include <validationinterface.h>

namespace finalization {

using FinalizationState = esperanza::FinalizationState;

CCriticalSection VoteRecorder::cs_recorder;
std::shared_ptr<VoteRecorder> VoteRecorder::g_voteRecorder;

void VoteRecorder::RecordVote(const esperanza::Vote &vote,
                              const std::vector<unsigned char> &voteSig,
                              const FinalizationState &fin_state) {

  LOCK(cs_recorder);

  // Check if the vote comes from a validator
  if (!fin_state.GetValidator(vote.m_validator_address)) {
    return;
  }

  boost::optional<VoteRecord> offendingVote = FindOffendingVote(vote);

  VoteRecord voteRecord{vote, voteSig};

  // Record the vote
  const auto validatorIt = voteRecords.find(vote.m_validator_address);
  if (validatorIt != voteRecords.end()) {
    validatorIt->second.emplace(vote.m_target_epoch, voteRecord);
  } else {
    std::map<uint32_t, VoteRecord> newMap;
    newMap.emplace(vote.m_target_epoch, voteRecord);
    voteRecords.emplace(vote.m_validator_address, std::move(newMap));
  }

  if (offendingVote) {
    esperanza::Result res = fin_state.IsSlashable(vote, offendingVote.get().vote);
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

  const auto cacheIt = voteCache.find(vote.m_validator_address);
  if (cacheIt != voteCache.end()) {
    if (cacheIt->second.vote == vote) {
      // Result was cached
      return boost::none;
    }
  }

  esperanza::Vote slashCandidate;
  const auto validatorIt = voteRecords.find(vote.m_validator_address);
  if (validatorIt != voteRecords.end()) {

    const auto &voteMap = validatorIt->second;

    // Check for double votes
    const auto recordIt = voteMap.find(vote.m_target_epoch);
    if (recordIt != voteMap.end()) {
      if (recordIt->second.vote.m_target_hash != vote.m_target_hash) {
        return recordIt->second;
      }
    }

    // Check for a surrounding vote
    for (auto it = voteMap.lower_bound(vote.m_source_epoch); it != voteMap.end(); ++it) {
      const VoteRecord &record = it->second;
      if (record.vote.m_source_epoch < vote.m_target_epoch) {
        if ((record.vote.m_source_epoch > vote.m_source_epoch &&
             record.vote.m_target_epoch < vote.m_target_epoch) ||
            (record.vote.m_source_epoch < vote.m_source_epoch &&
             record.vote.m_target_epoch > vote.m_target_epoch)) {
          return record;
        }
      }
    }
  }
  return boost::none;
}

boost::optional<VoteRecord> VoteRecorder::GetVote(const uint160 &validatorAddress, uint32_t epoch) const {

  LOCK(cs_recorder);
  const auto validatorIt = voteRecords.find(validatorAddress);
  if (validatorIt != voteRecords.end()) {
    const auto recordIt = validatorIt->second.find(epoch);
    if (recordIt != validatorIt->second.end()) {
      return recordIt->second;
    }
  }
  return boost::none;
}

void VoteRecorder::Init() {
  LOCK(cs_recorder);
  if (!g_voteRecorder) {
    //not using make_shared since the ctor is private
    g_voteRecorder = std::shared_ptr<VoteRecorder>(new VoteRecorder());
  }
}

void VoteRecorder::Reset() {
  LOCK(cs_recorder);
  g_voteRecorder.reset(new VoteRecorder());
}

std::shared_ptr<VoteRecorder> VoteRecorder::GetVoteRecorder() {
  return g_voteRecorder;
}

CScript VoteRecord::GetScript() const { return CScript::EncodeVote(vote, sig); }

bool RecordVote(const CTransaction &tx, CValidationState &err_state) {
  assert(tx.IsVote());

  LOCK(GetComponent<StateRepository>()->GetLock());
  const FinalizationState *fin_state = GetComponent<StateRepository>()->GetTipState();
  assert(fin_state != nullptr);

  esperanza::Vote vote;
  std::vector<unsigned char> voteSig;

  if (!CScript::ExtractVoteFromVoteSignature(tx.vin[0].scriptSig, vote, voteSig)) {
    return err_state.DoS(10, false, REJECT_INVALID, "bad-vote-data-format");
  }
  const esperanza::Result res = fin_state->ValidateVote(vote);

  if (res != +esperanza::Result::ADMIN_BLACKLISTED &&
      res != +esperanza::Result::VOTE_NOT_BY_VALIDATOR) {
    finalization::VoteRecorder::GetVoteRecorder()->RecordVote(vote, voteSig, *fin_state);
  }

  return true;
}

}  // namespace finalization
