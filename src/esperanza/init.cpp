// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/init.h>

#include <util.h>

namespace esperanza {

// clang-format off
std::string GetHelpString(bool showDebug) {
  std::string strUsage = HelpMessageGroup(_("Staking options:"));
  strUsage += HelpMessageOpt("-validating", "Stake your coins to become a validator (default: false)");
  strUsage += HelpMessageOpt("-proposing", "Whether to participate in proposing new blocks or not. Default: true");
  strUsage += HelpMessageOpt("-stakecombinemaximum", "Maximum numbers of coin to combine when proposing. Default: unlimited (0)");
  strUsage += HelpMessageOpt("-stakesplitthreshold", "Number of pieces stake should be split into when proposing. Default: 1");

  return strUsage;
}
// clang-format on

}  // namespace esperanza
