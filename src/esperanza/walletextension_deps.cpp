// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/walletextension_deps.h>

#include <injector.h>

namespace esperanza {

namespace {
Settings *SharedSettings() {
  static Settings settings;
  return &settings;
}
}  // namespace

WalletExtensionDeps::WalletExtensionDeps() noexcept
    : settings(SharedSettings()) {}

WalletExtensionDeps::WalletExtensionDeps(const UnitEInjector &injector) noexcept
    : settings(injector.GetSettings()) {}

}  // namespace esperanza
