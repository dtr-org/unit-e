// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/config.h>

namespace esperanza {

Config::Config(::ArgsManager &args, Config defaultConfig)
    // clang-format off
    : m_proposing(args.GetBoolArg("-proposing", defaultConfig.m_proposing)),
      m_numberOfProposerThreads(static_cast<size_t>(args.GetArg("-proposerthreads", static_cast<int64_t>(defaultConfig.m_numberOfProposerThreads)))),
      m_proposerSleep(std::chrono::milliseconds(args.GetArg("-proposersleep", static_cast<int64_t>(defaultConfig.m_proposerSleep.count())))),
      m_minProposeInterval(std::chrono::milliseconds(args.GetArg("-minproposeinterval", static_cast<int64_t>(defaultConfig.m_minProposeInterval.count())))) {}
// clang-format on

}  // namespace esperanza
