// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_FINALIZATIONPARAMS_H
#define UNITE_FINALIZATIONPARAMS_H

#include <amount.h>
#include <stdint.h>
#include <ufp64.h>

class CRegTestParams;

namespace esperanza {

class FinalizationParams final {

  friend ::CRegTestParams;

 public:
  //! Number of blocks between epochs
  int64_t m_epochLength = 50;

  CAmount m_minDepositSize = 1500 * UNIT;

  int64_t m_dynastyLogoutDelay = 700;

  int64_t m_withdrawalEpochDelay = static_cast<int>(1.5e4);

  int64_t m_slashFractionMultiplier = 3;

  int64_t m_bountyFractionDenominator = 25;

  ufp64::ufp64_t m_baseInterestFactor = ufp64::to_ufp64(7);

  ufp64::ufp64_t m_basePenaltyFactor = ufp64::div_2uint(2, 10000000);

  // UNIT-E: move this once we have a argManager util that parses parameters
  // passed to the node at startup
};

bool ParseFinalizationParams(std::string jsonString,
                             FinalizationParams &paramsOut);

}  // namespace esperanza

#endif  // UNITE_FINALIZATIONPARAMS_H
