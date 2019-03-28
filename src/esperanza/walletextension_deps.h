// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_ESPERANZA_WALLETEXTENSION_DEPS_H
#define UNIT_E_ESPERANZA_WALLETEXTENSION_DEPS_H

#include <dependency.h>
#include <settings.h>

class UnitEInjector;

namespace esperanza {

struct WalletExtensionDeps {
  Dependency<Settings> settings;

  WalletExtensionDeps() noexcept;
  explicit WalletExtensionDeps(const UnitEInjector &) noexcept;
};

}  // namespace esperanza

#endif  //UNIT_E_WALLETEXTENSION_DEPS_H
