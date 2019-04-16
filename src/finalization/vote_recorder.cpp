// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/vote_recorder.h>

#include <util.h>

namespace finalization {

//! The key for vote record in database.
//! * uin160 is validator address
//! * uint32_t is target epoch
using DBKey = std::pair<uint160, uint32_t>;

CCriticalSection VoteRecorder::cs_recorder;
std::shared_ptr<VoteRecorder> VoteRecorder::g_voteRecorder;

VoteRecorder::VoteRecorder(const DBParams &p)
    : m_db(GetDataDir() / "votes", p.cache_size, p.inmemory, p.wipe, p.obfuscate) {
  if (!p.wipe && !p.inmemory) {
    LoadFromDB();
  }
}

void VoteRecorder::LoadFromDB() {
  LOCK(cs_recorder);
  LogPrint(BCLog::FINALIZATION, "Restoring vote recorder from disk\n");
  uint32_t count = 0;
  voteRecords.clear();
  voteCache.clear();
  std::unique_ptr<CDBIterator> cursor(m_db.NewIterator());
  cursor->SeekToFirst();
  while (cursor->Valid()) {
    DBKey key;
    if (!cursor->GetKey(key)) {
      LogPrintf("WARN: cannot read next key from votes DB");
      return;
    }
    VoteRecord record;
    if (!cursor->GetValue(record)) {
      LogPrintf("WARN: cannot fetch data from votes DB, key=%s", util::to_string(key));
      return;
    }
    voteRecords[key.first].emplace(key.second, record);
    cursor->Next();
    ++count;
  }
  LogPrint(BCLog::FINALIZATION, "Loaded %d vote records\n", count);
}

void VoteRecorder::SaveVoteToDB(const VoteRecord &record) {
  LOCK(cs_recorder);
  DBKey key(record.vote.m_validator_address, record.vote.m_target_epoch);
  m_db.Write(key, record);
}

void VoteRecorder::RecordVote(const esperanza::Vote &vote,
                              const std::vector<unsigned char> &voteSig) {

  LOCK(cs_recorder);

  boost::optional<VoteRecord> offendingVote = FindOffendingVote(vote);

  VoteRecord voteRecord{vote, voteSig};

  // Record the vote
  bool saved_in_memory = false;
  const auto validatorIt = voteRecords.find(vote.m_validator_address);
  if (validatorIt != voteRecords.end()) {
    saved_in_memory = validatorIt->second.emplace(vote.m_target_epoch, voteRecord).second;
  } else {
    std::map<uint32_t, VoteRecord> newMap;
    newMap.emplace(vote.m_target_epoch, voteRecord);
    saved_in_memory = voteRecords.emplace(vote.m_validator_address, std::move(newMap)).second;
  }

  if (saved_in_memory) {
    SaveVoteToDB(voteRecord);
  }
}

boost::optional<VoteRecord> VoteRecorder::FindOffendingVote(const esperanza::Vote &vote) const {

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

uint32_t VoteRecorder::Count() {
  LOCK(cs_recorder);

  uint32_t count = 0;
  for (const auto &it : voteRecords) {
    count += it.second.size();
  }

  return count;
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

void VoteRecorder::Init(const DBParams &params) {
  LOCK(cs_recorder);
  if (!g_voteRecorder) {
    //not using make_shared since the ctor is private
    g_voteRecorder = std::shared_ptr<VoteRecorder>(new VoteRecorder(params));
  }
}

void VoteRecorder::Reset(const DBParams &params) {
  LOCK(cs_recorder);
  g_voteRecorder.reset(new VoteRecorder(params));
}

std::shared_ptr<VoteRecorder> VoteRecorder::GetVoteRecorder() {
  return g_voteRecorder;
}

CScript VoteRecord::GetScript() const { return CScript::EncodeVote(vote, sig); }

}  // namespace finalization
