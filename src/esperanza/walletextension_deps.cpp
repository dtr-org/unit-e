// Copyright (c) 2018-2019 The Unit-e developers
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
    : m_settings(SharedSettings()),
      m_finalization_state_repository(nullptr),
      m_active_chain(nullptr) {}

WalletExtensionDeps::WalletExtensionDeps(const Dependency<::Settings> settings) noexcept
    : m_settings(settings),
      m_finalization_state_repository(nullptr),
      m_active_chain(nullptr) {}

WalletExtensionDeps::WalletExtensionDeps(const UnitEInjector &injector) noexcept
    : m_settings(injector.Get<Settings>()),
      m_finalization_state_repository(injector.Get<finalization::StateRepository>()),
      m_active_chain(injector.Get<staking::ActiveChain>()) {}

}  // namespace esperanza
