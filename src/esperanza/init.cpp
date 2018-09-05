// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <esperanza/init.h>

#include <util.h>

namespace esperanza {

// clang-format off
std::string GetHelpString(bool showDebug) {
  std::string strUsage = HelpMessageGroup(_("Staking options:"));
  strUsage += HelpMessageOpt("-validating", "Stake your coins to become a validator (default: false)");
  strUsage += HelpMessageOpt("-propose", "Whether to participate in proposing new blocks via staking or not. Default: true");
  strUsage += HelpMessageOpt("-minproposeinterval", "Default: 0");
  strUsage += HelpMessageOpt("-proposersleep", "Default: 500ms");
  strUsage += HelpMessageOpt("-proposerthreads", "The number of threads used to mine. Maximum of 1 per Wallet. If there are more wallets than threads, staking will be distributed across the threads. Default: 1");
  return strUsage;
}
// clang-format on

} // namespace esperanza
