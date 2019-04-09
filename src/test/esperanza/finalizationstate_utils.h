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

class FinalizationStateSpy : public FinalizationState {
 private:
  const FinalizationParams params = CreateChainParams(CBaseChainParams::TESTNET)->GetFinalization();

 public:
  FinalizationStateSpy(const FinalizationParams &_params,
                       const AdminParams &adminParams) : FinalizationState(_params, adminParams),
                                                         params(_params) {}
  FinalizationStateSpy() : FinalizationState(params, AdminParams()) {}
  FinalizationStateSpy(const FinalizationParams &_params) : FinalizationState(_params, AdminParams()),
                                                            params(_params) {}
  FinalizationStateSpy(const FinalizationStateSpy &parent) : FinalizationState(parent) {}

  CAmount *CurDynDeposits() { return &m_cur_dyn_deposits; }
  CAmount *PrevDynDeposits() { return &m_prev_dyn_deposits; }
  uint64_t *RewardFactor() { return &m_reward_factor; }
  std::map<uint160, Validator> &Validators() { return m_validators; }
  std::map<uint160, Validator> *pValidators() { return &m_validators; }
  std::map<uint32_t, Checkpoint> &Checkpoints() { return m_checkpoints; }
  void SetRecommendedTarget(const CBlockIndex &block_index) {
    m_recommended_target_hash = block_index.GetBlockHash();
    m_recommended_target_epoch = GetEpoch(block_index);
  }
  void SetExpectedSourceEpoch(uint32_t epoch) {
    m_expected_source_epoch = epoch;
  }
  void SetLastFinalizedEpoch(uint32_t epoch) {
    m_checkpoints[epoch].m_is_finalized = true;
    m_last_finalized_epoch = epoch;
  }

  uint32_t EpochLength() const { return m_settings.epoch_length; }
  CAmount MinDepositSize() const { return m_settings.min_deposit_size; }
  int64_t DynastyLogoutDelay() const { return m_settings.dynasty_logout_delay; }
  int64_t WithdrawalEpochDelay() const {
    return m_settings.withdrawal_epoch_delay;
  }
  int64_t BountyFractionDenominator() const {
    return m_settings.bounty_fraction_denominator;
  }

  void CreateAndActivateDeposit(const uint160 &validator_address, CAmount deposit_size);

  void shuffle();

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
CPubKey MakePubKey();
esperanza::AdminKeySet MakeKeySet();

#endif  //UNIT_E_TESTS_FINALIZATIONSTATE_UTILS_H
