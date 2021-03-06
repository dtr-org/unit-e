// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <random.h>
#include <test/esperanza/finalizationstate_utils.h>

uint160 RandValidatorAddr() {
  CKey key;
  key.MakeNewKey(true);
  return key.GetPubKey().GetID();
}

CPubKey MakePubKey() {
  CKey key;
  key.MakeNewKey(true);
  return key.GetPubKey();
}

esperanza::AdminKeySet MakeKeySet() {
  return {{MakePubKey(), MakePubKey(), MakePubKey()}};
}

template <typename T>
T Rand() {
  return static_cast<T>(GetRand(std::numeric_limits<T>::max()));
}

template <>
bool Rand<bool>() {
  return static_cast<bool>(GetRand(2));
}

#define ConstRand(N) [] { static size_t r = GetRand(N); return r; }()

void FinalizationStateSpy::shuffle() {
  for (size_t i = 0; i < ConstRand(5); ++i) {
    m_checkpoints[i].m_is_justified = Rand<bool>();
    m_checkpoints[i].m_is_finalized = Rand<bool>();
    m_checkpoints[i].m_cur_dynasty_deposits = Rand<uint64_t>();
    m_checkpoints[i].m_prev_dynasty_deposits = Rand<uint64_t>();
    {
      for (size_t j = 0; j < ConstRand(5); ++j) {
        m_checkpoints[j].m_cur_dynasty_votes[j] = Rand<uint64_t>();
      }
    }
    for (size_t j = 0; j < ConstRand(5); ++j) {
      m_checkpoints[i].m_prev_dynasty_votes[j] = Rand<uint64_t>();
    }
    for (size_t j = 0; j < ConstRand(5); ++j) {
      uint160 hash;
      GetRandBytes((unsigned char *)&hash, sizeof(hash));
      m_checkpoints[i].m_vote_set.emplace(hash);
    }
  }
  for (size_t i = 0; i < ConstRand(5); ++i) {
    m_dynasty_start_epoch[i] = Rand<uint32_t>();
  }
  for (size_t i = 0; i < ConstRand(5); ++i) {
    uint160 v;
    GetRandBytes((unsigned char *)&v, sizeof(v));
    m_validators[v].m_validator_address = v;
    m_validators[v].m_deposit = Rand<uint64_t>();
    m_validators[v].m_start_dynasty = Rand<uint32_t>();
    m_validators[v].m_end_dynasty = Rand<uint32_t>();
    m_validators[v].m_is_slashed = Rand<bool>();
    m_validators[v].m_deposits_at_logout = Rand<uint64_t>();
    m_validators[v].m_last_transaction_hash = GetRandHash();
  }
  for (size_t i = 0; i < ConstRand(5); ++i) {
    m_dynasty_deltas[i] = Rand<CAmount>();
  }
  for (size_t i = 0; i < ConstRand(5); ++i) {
    m_deposit_scale_factor[i] = Rand<ufp64::ufp64_t>();
  }
  for (size_t i = 0; i < ConstRand(5); ++i) {
    m_total_slashed[i] = Rand<CAmount>();
  }
  m_current_epoch = Rand<uint32_t>();
  m_current_dynasty = Rand<uint32_t>();
  m_cur_dyn_deposits = Rand<CAmount>();
  m_prev_dyn_deposits = Rand<CAmount>();
  m_expected_source_epoch = Rand<uint32_t>();
  m_last_finalized_epoch = Rand<uint32_t>();
  m_last_justified_epoch = Rand<uint32_t>();
  m_recommended_target_hash = GetRandHash();
  m_recommended_target_epoch = Rand<uint32_t>();
  m_reward_factor = Rand<ufp64::ufp64_t>();
}

#undef ConstRand

uint32_t FinalizationStateSpy::GetExpectedSourceEpoch() const {
  return m_expected_source_epoch;
}

void FinalizationStateSpy::CreateAndActivateDeposit(const uint160 &validator_address, CAmount deposit_size) {
  BOOST_REQUIRE_EQUAL(GetCurrentEpoch(), 0);

  CreateDeposit(validator_address, deposit_size);

  for (uint32_t i = 1; i < 4 * EpochLength() + 1; i += EpochLength()) {
    BOOST_REQUIRE_EQUAL(GetActiveFinalizers().size(), 0);

    // recommended target epoch in ProcessNewCommits
    // when checkpoint is being processed
    m_recommended_target_epoch = m_current_epoch;

    Result res = InitializeEpoch(i);
    BOOST_REQUIRE_EQUAL(res, +Result::SUCCESS);
  }

  BOOST_REQUIRE_EQUAL(GetCurrentDynasty(), 2);
  BOOST_REQUIRE_EQUAL(GetCurrentEpoch(), 4);
  BOOST_REQUIRE_EQUAL(GetLastJustifiedEpoch(), 2);
  BOOST_REQUIRE_EQUAL(GetLastFinalizedEpoch(), 2);
  BOOST_REQUIRE(!GetActiveFinalizers().empty());
  BOOST_REQUIRE_EQUAL(m_expected_source_epoch, 2);
  BOOST_REQUIRE_EQUAL(m_recommended_target_epoch, 3);
}

void FinalizationStateSpy::CreateDeposit(const uint160 &finalizer_address, CAmount deposit_size) {
  Result res = ValidateDeposit(finalizer_address, deposit_size);
  BOOST_REQUIRE_EQUAL(res, +Result::SUCCESS);
  ProcessDeposit(finalizer_address, deposit_size);
}
