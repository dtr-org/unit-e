// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/settings_init.h>

#include <mutex>

namespace esperanza {

static std::mutex initLock;
static std::unique_ptr<Settings> settings = nullptr;

const Settings *InitSettings(::ArgsManager &args) {
  std::unique_lock<decltype(initLock)> lock;
  if (settings) {
    return nullptr;
  }
  settings.reset(new Settings(args));
  return settings.get();
}

}  // namespace esperanza
