// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/init.h>

#include <util.h>

namespace esperanza {

// clang-format off
void AddOptions(ArgsManager &args) {
  std::string strUsage = HelpMessageGroup(_(":"));
  gArgs.AddArg("-proposing", "Whether to participate in proposing new blocks or not. Default: true", false, OptionsCategory::STAKING);
  gArgs.AddArg("-permissioning", "Whether to start with permissioning enabled (works only on regtest). Default: false", false, OptionsCategory::STAKING);
  gArgs.AddArg("-stakecombinemaximum", "Maximum amount to combine when proposing. Default: unlimited (0)", false, OptionsCategory::STAKING);
  gArgs.AddArg("-stakesplitthreshold", "Maximum amount a single coinbase output should have. Default: unlimited (0)", false, OptionsCategory::STAKING);
  gArgs.AddArg("-validating", "Stake your coins to become a validator (default: false)", false, OptionsCategory::STAKING);
  gArgs.AddArg("-rewardaddress=<addr>", "Address to which any reward from block proposing should be sent to, if not set the destination of the staking coin will be chosen", false, OptionsCategory::STAKING);
}
// clang-format on

}  // namespace esperanza
