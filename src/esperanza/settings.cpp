// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/settings.h>

namespace esperanza {

Settings::Settings(::ArgsManager &args) : Settings(args, Settings::Default()) {}

//! initializes the settings by reading them from an args manager.
Settings::Settings(::ArgsManager &args, const Settings &defaultConfig)
    : m_proposing(args.GetBoolArg("-proposing", defaultConfig.m_proposing)),
      m_validating(args.GetBoolArg("-validating", defaultConfig.m_validating)),
      m_numberOfProposerThreads(static_cast<size_t>(args.GetArg(
          "-proposerthreads",
          static_cast<int64_t>(defaultConfig.m_numberOfProposerThreads)))),
      m_proposerSleep(std::chrono::milliseconds(args.GetArg(
          "-proposersleep",
          static_cast<int64_t>(defaultConfig.m_proposerSleep.count())))),
      m_minProposeInterval(std::chrono::milliseconds(args.GetArg(
          "-minproposeinterval",
          static_cast<int64_t>(defaultConfig.m_minProposeInterval.count())))) {}

}  // namespace esperanza
