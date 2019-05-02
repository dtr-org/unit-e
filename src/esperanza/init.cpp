// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/init.h>

#include <util.h>

namespace esperanza {

// clang-format off
std::string GetHelpString(bool showDebug) {
  std::string strUsage = HelpMessageGroup(_("Staking options:"));
  strUsage += HelpMessageOpt("-proposing", "Whether to participate in proposing new blocks or not. Default: true");
  strUsage += HelpMessageOpt("-permissioning", "Whether to start with permissioning enabled (works only on regtest). Default: false");
  strUsage += HelpMessageOpt("-stakecombinemaximum", "Maximum amount to combine when proposing. Default: unlimited (0)");
  strUsage += HelpMessageOpt("-stakesplitthreshold", "Maximum amount a single coinbase output should have. Default: unlimited (0)");
  strUsage += HelpMessageOpt("-validating", "Stake your coins to become a validator (default: false)");
  strUsage += HelpMessageOpt("-rewardaddress=<addr>", "Address to which any reward from block proposing should be sent to, if not set the destination of the staking coin will be chosen");
  strUsage += HelpMessageOpt("-finalizervotefromepochblocknumber=<n>", "From which block in the epoch finalizer must start voting");

  return strUsage;
}
// clang-format on

}  // namespace esperanza
