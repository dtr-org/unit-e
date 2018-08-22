// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNITE_ESPERANZA_CONFIG_H
#define UNITE_ESPERANZA_CONFIG_H

#include <util.h>
#include <cstddef>

namespace esperanza {

struct Config {

  //! Whether this node should stake or not.
  bool m_staking;

  //! How many thread to use for staking. At least 1, at most number of wallets.
  size_t m_numberOfStakeThreads;

  int64_t m_minerSleep;

  int64_t m_minStakeInterval;

  Config(bool staking, size_t numberOfStakeThreads, int64_t minerSleep, int64_t minStakeInterval) noexcept;

  Config(::ArgsManager &args, Config defaultConfig = Config());

  Config() noexcept;

};

// todo: get rid of global config in favor of injecting it into components for better testing
extern Config g_config;

} // namespace esperanza

#endif // UNITE_ESPERANZA_CONFIG_H
