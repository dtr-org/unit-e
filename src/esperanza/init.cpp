// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/init.h>

#include <util/system.h>

namespace esperanza {

// clang-format off
void AddOptions(ArgsManager &args) {
  args.AddArg("-proposing", "Whether to participate in proposing new blocks or not. Default: true", false, OptionsCategory::STAKING);
  args.AddArg("-permissioning", "Whether to start with permissioning enabled (works only on regtest). Default: false", false, OptionsCategory::STAKING);
  args.AddArg("-stakecombinemaximum", "Maximum amount to combine when proposing. Default: unlimited (0)", false, OptionsCategory::STAKING);
  args.AddArg("-stakesplitthreshold", "Maximum amount a single coinbase output should have. Default: unlimited (0)", false, OptionsCategory::STAKING);
  args.AddArg("-validating", "Stake your coins to become a validator (default: false)", false, OptionsCategory::STAKING);
  args.AddArg("-rewardaddress=<addr>", "Address to which any reward from block proposing should be sent to, if not set the destination of the staking coin will be chosen", false, OptionsCategory::STAKING);
  args.AddArg("-finalizervotefromepochblocknumber=<n>", "From which block in the epoch finalizer must start voting", false, OptionsCategory::STAKING);
}
// clang-format on

}  // namespace esperanza
