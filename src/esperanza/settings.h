// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_CONFIG_H
#define UNITE_ESPERANZA_CONFIG_H

#include <util.h>

#include <chrono>
#include <cstddef>

namespace esperanza {

struct Settings {
  //! Whether this node should propose blocks or not.
  bool m_proposing = true;

  //! Whether this node should be a validator.
  bool m_validating = false;

  Settings(::ArgsManager &args);

  Settings(::ArgsManager &args, const Settings &defaultConfig);

  // clang-tidy recommends `Config() noexcept = default`, but then `clang`
  // [sic!] fails to compile it. Removing the `noexcept` makes it work, but.
  Settings() noexcept {};  // NOLINT(modernize-use-equals-default)

  static const Settings &Default() {
    static Settings settings;
    return settings;
  }
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_CONFIG_H
