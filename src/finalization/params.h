// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_FINALIZATION_PARAMS_H
#define UNITE_FINALIZATION_PARAMS_H

#include <amount.h>
#include <blockchain/blockchain_types.h>
#include <dependency.h>
#include <esperanza/adminparams.h>
#include <injector_config.h>
#include <ufp64.h>
#include <util/system.h>

#include <boost/optional.hpp>

#include <memory>

namespace finalization {

struct Params {
  uint32_t epoch_length = 5;
  CAmount min_deposit_size = 1500 * UNIT;
  int64_t dynasty_logout_delay = 2;
  int64_t withdrawal_epoch_delay = 5;
  int64_t slash_fraction_multiplier = 3;
  int64_t bounty_fraction_denominator = 25;
  ufp64::ufp64_t base_interest_factor = ufp64::to_ufp64(7);
  ufp64::ufp64_t base_penalty_factor = ufp64::div_2uint(2, 100000);

  esperanza::AdminParams admin_params;

  //! \brief Returns the epoch which includes block_height.
  inline uint32_t GetEpoch(const blockchain::Height block_height) const {
    uint32_t epoch = block_height / epoch_length;
    if (block_height % epoch_length != 0) {
      ++epoch;
    }
    return epoch;
  }

  //! \brief Returns the height of the first block of the epoch.
  inline blockchain::Height GetEpochStartHeight(const uint32_t epoch) const {
    // epoch=0 contains only genesis
    if (epoch == 0) {
      return 0;
    }
    return GetEpochCheckpointHeight(epoch - 1) + 1;
  }

  //! \brief Returns the height of the last block of the epoch.
  inline blockchain::Height GetEpochCheckpointHeight(const uint32_t epoch) const {
    return epoch * epoch_length;
  }

  //! \brief Returns whether block at block_height is the first block of the epoch.
  inline bool IsEpochStart(blockchain::Height block_height) const {
    return block_height % epoch_length == 1;
  }

  //! \brief Returns whether block at block_height is the last block of the epoch.
  inline bool IsCheckpoint(blockchain::Height block_height) const {
    return block_height % epoch_length == 0;
  }

  static Params RegTest(bool gen_admin_keys = false);
  static Params TestNet(bool gen_admin_keys = false);

  static std::unique_ptr<Params> New(Dependency<UnitEInjectorConfiguration>,
                                     Dependency<ArgsManager>);
};

}  // namespace finalization

#endif
