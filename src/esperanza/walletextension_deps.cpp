// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/walletextension_deps.h>
#include <injector.h>
#include <staking/stake_validator.h>

namespace esperanza {

namespace {
Settings *SharedSettings() {
  static Settings settings;
  return &settings;
}
}  // namespace

WalletExtensionDeps::WalletExtensionDeps() noexcept
    : settings(SharedSettings()),
      m_stake_validator(nullptr) {}

WalletExtensionDeps::WalletExtensionDeps(const Dependency<staking::StakeValidator> stake_validator) noexcept
    : settings(SharedSettings()),
      m_stake_validator(stake_validator) {}

WalletExtensionDeps::WalletExtensionDeps(const UnitEInjector &injector) noexcept
    : settings(injector.Get<Settings>()),
      m_stake_validator(injector.Get<staking::StakeValidator>()) {}

}  // namespace esperanza
