// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_FINALIZATIONPARAMS_H
#define UNITE_FINALIZATIONPARAMS_H

#include <amount.h>
#include <blockchain/blockchain_types.h>
#include <stdint.h>
#include <ufp64.h>

class CRegTestParams;

namespace esperanza {

struct FinalizationParams {

  FinalizationParams();

  //! Number of blocks in one epoch
  uint32_t epoch_length;

  CAmount min_deposit_size;

  int64_t dynasty_logout_delay;

  int64_t withdrawal_epoch_delay;

  int64_t slash_fraction_multiplier;

  int64_t bounty_fraction_denominator;

  ufp64::ufp64_t base_interest_factor;

  ufp64::ufp64_t base_penalty_factor;

  blockchain::Height GetEpochStartHeight(const uint32_t epoch) const {
    // epoch=0 contains only genesis
    if (epoch == 0) {
      return 0;
    }

    return GetEpochCheckpointHeight(epoch - 1) + 1;
  }

  blockchain::Height GetEpochCheckpointHeight(const uint32_t epoch) const {
    return epoch * epoch_length;
  }

  // UNIT-E: move this once we have a argManager util that parses parameters
  // passed to the node at startup
};

bool ParseFinalizationParams(const std::string &json_string, FinalizationParams &params_out);

}  // namespace esperanza

#endif  // UNITE_FINALIZATIONPARAMS_H
