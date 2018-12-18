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
  strUsage += HelpMessageOpt("-stakecombinemaximum", "Maximum amount to combine when proposing. Default: unlimited (0)");
  strUsage += HelpMessageOpt("-stakesplitthreshold", "Maximum amount a single coinbase output should have. Default: unlimited (0)");
  strUsage += HelpMessageOpt("-validating", "Stake your coins to become a validator (default: false)");

  return strUsage;
}
// clang-format on

}  // namespace esperanza
