// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <esperanza/init.h>

#include <util.h>

namespace esperanza {

std::string GetHelpString(bool showDebug) {
  std::string strUsage = HelpMessageGroup(_("Staking options:"));
  strUsage += HelpMessageOpt("-staking", "Whether to participate in proposing new blocks via staking or not. Default: true");
  strUsage += HelpMessageOpt("-minstakeinterval", "Default: 0");
  strUsage += HelpMessageOpt("-minersleep", "Default: 500ms");
  strUsage += HelpMessageOpt("-stakingthreads", "The number of threads used to mine. Maximum of 1 per Wallet. If there are more wallets than threads, staking will be distributed across the threads. Default: 1");
  return strUsage;
}

} // namespace esperanza
