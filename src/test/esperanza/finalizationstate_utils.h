#ifndef UNIT_E_TESTS_FINALIZATIONSTATE_UTILS_H
#define UNIT_E_TESTS_FINALIZATIONSTATE_UTILS_H

#include <esperanza/finalizationstate.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <random.h>
#include <test/test_unite.h>

using namespace esperanza;

const FinalizationParams params{};

class FinalizationStateSpy : public FinalizationState {
 public:
  FinalizationStateSpy() : FinalizationState(params, AdminParams()) {}

  uint64_t *CurDynDeposits() { return &m_curDynDeposits; }
  uint64_t *PrevDynDeposits() { return &m_prevDynDeposits; }
  uint64_t *RewardFactor() { return &m_rewardFactor; }
  std::map<uint256, Validator> &Validators() { return m_validators; }
  std::map<uint256, Validator> *pValidators() { return &m_validators; }
  std::map<uint32_t, Checkpoint> &Checkpoints() {
    return const_cast<std::map<uint32_t, Checkpoint> &>(m_checkpoints);
  }
  uint256 *RecommendedTargetHash() { return &m_recommendedTargetHash; }

  int64_t EpochLength() const { return m_settings.m_epochLength; }
  CAmount MinDepositSize() const { return m_settings.m_minDepositSize; }
  int64_t DynastyLogoutDelay() const { return m_settings.m_dynastyLogoutDelay; }
  int64_t WithdrawalEpochDelay() const {
    return m_settings.m_withdrawalEpochDelay;
  }
  int64_t BountyFractionDenominator() const {
    return m_settings.m_bountyFractionDenominator;
  }

  using FinalizationState::GetCurrentDynasty;
  using FinalizationState::GetDepositSize;
  using FinalizationState::InitializeEpoch;
  using FinalizationState::ProcessDeposit;
  using FinalizationState::ProcessLogout;
  using FinalizationState::ProcessSlash;
  using FinalizationState::ProcessVote;
  using FinalizationState::ProcessWithdraw;
};


#endif //UNIT_E_TESTS_FINALIZATIONSTATE_UTILS_H
