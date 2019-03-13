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
   * uint64_t - is enough to represent any amount of UNIT-E coins
   * (total_supply=(e * 10^17) and log2(total_supply)=~58 )
   * ufp64_t - is a way to represent a decimal number with integer part up to
   * 10E9 and decimal part with precision of 10E-8. Using this type is safe as
   * long as the above conditions are met. For example multiplications between
   * ufp64t and uint64_t are for example safe since for the intermediate step a
   * bigger int type is used, but if the result is not representable by 32 bits
   * then the final value will overflow.
   */

  // Map of epoch number to checkpoint
  std::map<uint32_t, Checkpoint> m_checkpoints;

  // Map of epoch number to dynasty number
  std::map<uint32_t, uint32_t> m_epochToDynasty;

  // Map of dynasty number to the starting epoch number
  std::map<uint32_t, uint32_t> m_dynastyStartEpoch;

  // List of validators
  std::map<uint160, Validator> m_validators;

  // Map of the dynasty number with the delta in deposits with the previous one
  std::map<uint32_t, uint64_t> m_dynastyDeltas;

  // Map of the epoch number with the deposit scale factor
  std::map<uint32_t, ufp64::ufp64_t> m_depositScaleFactor;

  // Map of the epoch number with the running total of deposits slashed
  std::map<uint32_t, uint64_t> m_totalSlashed;

  // The current epoch number
  uint32_t m_currentEpoch = 0;

  // The current dynasy number
  uint32_t m_currentDynasty = 0;

  // Total scaled deposits in the current dynasty
  uint64_t m_curDynDeposits = 0;

  // Total scaled deposits in the previous dynasty
  uint64_t m_prevDynDeposits = 0;

  // Expected epoch of the vote source
  uint32_t m_expectedSourceEpoch = 0;

  // Number of the last finalized epoch
  uint32_t m_lastFinalizedEpoch = 0;

  // Number of the last justified epoch
  uint32_t m_lastJustifiedEpoch = 0;

  // Last checkpoint
  uint256 m_recommendedTargetHash = uint256();
  uint32_t m_recommendedTargetEpoch = 0;

  ufp64::ufp64_t m_lastVoterRescale = 0;

  ufp64::ufp64_t m_lastNonVoterRescale = 0;

  // Reward for voting as fraction of deposit size
  ufp64::ufp64_t m_rewardFactor = 0;

  AdminState m_adminState;
};

}  // namespace esperanza

#endif
