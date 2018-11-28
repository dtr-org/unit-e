// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/settings.h>

namespace esperanza {

Settings::Settings(::ArgsManager &args) : Settings(args, Settings::Default()) {}

//! initializes the settings by reading them from an args manager.
Settings::Settings(::ArgsManager &args, const Settings &defaultConfig)
    : m_validating(args.GetBoolArg("-validating", defaultConfig.m_validating)) {
}

}  // namespace esperanza
