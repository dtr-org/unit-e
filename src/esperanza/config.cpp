// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <esperanza/config.h>

namespace esperanza {

Config::Config(bool staking,
               size_t numberOfStakeThreads,
               int64_t minerSleep,
               int64_t minStakeInterval) noexcept
    : m_staking(staking),
      m_numberOfStakeThreads(numberOfStakeThreads),
      m_minerSleep(minerSleep),
      m_minStakeInterval(minStakeInterval) {};

Config::Config(::ArgsManager &args, Config defaultConfig)
    : m_staking(args.GetBoolArg("-staking", defaultConfig.m_staking)),
      m_numberOfStakeThreads(static_cast<size_t>(args.GetArg("-stakingthreads",
                                                             static_cast<int64_t>(defaultConfig.m_numberOfStakeThreads)))),
      m_minerSleep(args.GetArg("-minersleep", defaultConfig.m_minerSleep)),
      m_minStakeInterval(args.GetArg("-minstakeinterval", defaultConfig.m_minStakeInterval)) {}

Config::Config() noexcept : Config(true, 1, 500, 0) {};

} // namespace esperanza
