// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_PROPOSER_SETTINGS_H
#define UNIT_E_PROPOSER_PROPOSER_SETTINGS_H

#include <amount.h>
#include <util.h>

#include <cstddef>
#include <memory>

namespace proposer {

struct Settings {

  //! How many threads to use for proposing. At least 1, at most number of
  //! wallets.
  size_t m_numberOfProposerThreads = 1;

  std::chrono::milliseconds m_proposerSleep = std::chrono::seconds(30);

  //! Minimum interval between proposing blocks
  std::chrono::milliseconds m_minProposeInterval = std::chrono::seconds(4);

  std::string m_proposerThreadName = "proposer";

  //! for regtest, don't stake above nStakeLimitHeight
  int m_stakeLimitHeight = 0;

  CAmount m_stakeCombineThreshold = 1000 * UNIT;

  CAmount m_stakeSplitThreshold = 2000 * UNIT;

  size_t m_maxStakeCombine = 3;

  static std::unique_ptr<Settings> MakeSettings() {
    return MakeUnique<Settings>();
  }
};

}  // namespace proposer

#endif  // UNIT_E_PROPOSER_SETTINGS_H
