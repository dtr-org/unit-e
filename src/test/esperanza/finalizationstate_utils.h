// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_TESTS_FINALIZATIONSTATE_UTILS_H
#define UNIT_E_TESTS_FINALIZATIONSTATE_UTILS_H

#include <esperanza/finalizationstate.h>
#include <random.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

using namespace esperanza;

const FinalizationParams params{};

const CAmount MIN_DEPOSIT_SIZE = 100000 * UNIT;
const int64_t EPOCH_LENGTH = 50;

class FinalizationStateSpy : public FinalizationState {
 public:
  FinalizationStateSpy() : FinalizationState(params, AdminParams()) {}

  uint64_t *CurDynDeposits() { return &m_curDynDeposits; }
  uint64_t *PrevDynDeposits() { return &m_prevDynDeposits; }
  uint64_t *RewardFactor() { return &m_rewardFactor; }
  std::map<uint160, Validator> &Validators() { return m_validators; }
  std::map<uint160, Validator> *pValidators() { return &m_validators; }
  std::map<uint32_t, Checkpoint> &Checkpoints() {
    return const_cast<std::map<uint32_t, Checkpoint> &>(m_checkpoints);
  }
  uint256 *RecommendedTargetHash() { return &m_recommendedTargetHash; }

  uint32_t EpochLength() const { return m_settings.m_epochLength; }
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

uint160 RandValidatorAddr();

#endif  //UNIT_E_TESTS_FINALIZATIONSTATE_UTILS_H
