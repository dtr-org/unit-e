// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_FINALIZATIONSTATE_DATA_H
#define UNITE_ESPERANZA_FINALIZATIONSTATE_DATA_H

#include <esperanza/adminstate.h>
#include <esperanza/checkpoint.h>
#include <esperanza/validator.h>
#include <serialize.h>
#include <ufp64.h>
#include <uint256.h>

namespace esperanza {

/**
 * This class is the base data-class with all the data required by
 * FinalizationState. If you need to add new data member to FinalizationState
 * you probably would add it here.
 */
class FinalizationStateData {
 public:
  bool operator==(const FinalizationStateData &other) const;

 protected:
  FinalizationStateData(const AdminParams &adminParams);
  FinalizationStateData(const FinalizationStateData &) = default;
  FinalizationStateData(FinalizationStateData &&) = default;
  ~FinalizationStateData() = default;

  /**
   * A quick comment on the types chosen to represent the various class members:
   * uint32_t - is enough to represent any epoch (even with one epoch a second
   * it would last ~136 yrs)
   * (total_supply=(e * 10^17) and log2(total_supply)=~58 )
   * ufp64_t - is a way to represent a decimal number with integer part up to
   * 10E9 and decimal part with precision of 10E-8. Using this type is safe as
   * long as the above conditions are met. For example multiplications between
   * ufp64t and uint64_t are safe since for the intermediate step a bigger int
   * type is used, but if the result is not representable by 32 bits then the
   * final value will overflow.
   */

  // Map of epoch number to checkpoint
  std::map<uint32_t, Checkpoint> m_checkpoints;

  // Map of dynasty number to the starting epoch number
  std::map<uint32_t, uint32_t> m_dynasty_start_epoch;

  // List of validators
  std::map<uint160, Validator> m_validators;

  // Map of the dynasty number with the delta in deposits with the previous one
  std::map<uint32_t, CAmount> m_dynasty_deltas;

  // Map of the epoch number with the deposit scale factor
  std::map<uint32_t, ufp64::ufp64_t> m_deposit_scale_factor;

  // Map of the epoch number with the running total of deposits slashed
  std::map<uint32_t, CAmount> m_total_slashed;

  // The current epoch number
  uint32_t m_current_epoch = 0;

  // The current dynasy number
  uint32_t m_current_dynasty = 0;

  // Total scaled deposits in the current dynasty
  CAmount m_cur_dyn_deposits = 0;

  // Total scaled deposits in the previous dynasty
  CAmount m_prev_dyn_deposits = 0;

  // Expected epoch of the vote source
  uint32_t m_expected_source_epoch = 0;

  // Number of the last finalized epoch
  uint32_t m_last_finalized_epoch = 0;

  // Number of the last justified epoch
  uint32_t m_last_justified_epoch = 0;

  // Last checkpoint
  uint256 m_recommended_target_hash = uint256();
  uint32_t m_recommended_target_epoch = 0;

  ufp64::ufp64_t m_last_voter_rescale = 0;

  ufp64::ufp64_t m_last_non_voter_rescale = 0;

  // Reward for voting as fraction of deposit size
  ufp64::ufp64_t m_reward_factor = 0;

  AdminState m_admin_state;

 public:
  ADD_SERIALIZE_METHODS

  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_checkpoints);
    READWRITE(m_dynasty_start_epoch);
    READWRITE(m_validators);
    READWRITE(m_dynasty_deltas);
    READWRITE(m_deposit_scale_factor);
    READWRITE(m_total_slashed);
    READWRITE(m_current_epoch);
    READWRITE(m_current_dynasty);
    READWRITE(m_cur_dyn_deposits);
    READWRITE(m_prev_dyn_deposits);
    READWRITE(m_expected_source_epoch);
    READWRITE(m_last_finalized_epoch);
    READWRITE(m_last_justified_epoch);
    READWRITE(m_recommended_target_hash);
    READWRITE(m_recommended_target_epoch);
    READWRITE(m_last_voter_rescale);
    READWRITE(m_last_non_voter_rescale);
    READWRITE(m_reward_factor);
    READWRITE(m_admin_state);
  };

  std::string ToString() const;
};

}  // namespace esperanza

#endif
