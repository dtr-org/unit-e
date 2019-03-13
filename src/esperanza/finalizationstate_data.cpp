#include <esperanza/finalizationstate_data.h>

namespace esperanza {

FinalizationStateData::FinalizationStateData(const AdminParams &adminParams)
    : m_adminState(adminParams) {}

bool FinalizationStateData::operator==(const FinalizationStateData &other) const {
  return m_checkpoints == other.m_checkpoints &&
         m_epochToDynasty == other.m_epochToDynasty &&
         m_dynastyStartEpoch == other.m_dynastyStartEpoch &&
         m_validators == other.m_validators &&
         m_dynastyDeltas == other.m_dynastyDeltas &&
         m_depositScaleFactor == other.m_depositScaleFactor &&
         m_totalSlashed == other.m_totalSlashed &&
         m_currentEpoch == other.m_currentEpoch &&
         m_currentDynasty == other.m_currentDynasty &&
         m_curDynDeposits == other.m_curDynDeposits &&
         m_prevDynDeposits == other.m_prevDynDeposits &&
         m_expectedSourceEpoch == other.m_expectedSourceEpoch &&
         m_lastFinalizedEpoch == other.m_lastFinalizedEpoch &&
         m_lastJustifiedEpoch == other.m_lastJustifiedEpoch &&
         m_recommendedTargetHash == other.m_recommendedTargetHash &&
         m_recommendedTargetEpoch == other.m_recommendedTargetEpoch &&
         m_lastVoterRescale == other.m_lastVoterRescale &&
         m_lastNonVoterRescale == other.m_lastNonVoterRescale &&
         m_rewardFactor == other.m_rewardFactor &&
         m_adminState == other.m_adminState;
}

}  // namespace esperanza
