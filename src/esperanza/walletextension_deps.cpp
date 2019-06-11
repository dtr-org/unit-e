// Copyright (c) 2018-2019 The Unit-e developers
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
    : m_settings(SharedSettings()),
      m_finalization_state_repository(nullptr),
      m_active_chain(nullptr),
      m_stake_validator(nullptr),
      m_finalization_reward_logic(nullptr) {}

WalletExtensionDeps::WalletExtensionDeps(
    const Dependency<::Settings> settings,
    const Dependency<staking::StakeValidator> stake_validator,
    const Dependency<proposer::FinalizationRewardLogic> finalization_reward_logic) noexcept
    : m_settings(settings),
      m_finalization_state_repository(nullptr),
      m_active_chain(nullptr),
      m_stake_validator(stake_validator),
      m_finalization_reward_logic(finalization_reward_logic) {}

WalletExtensionDeps::WalletExtensionDeps(const UnitEInjector &injector) noexcept
    : m_settings(injector.Get<Settings>()),
      m_finalization_state_repository(injector.Get<finalization::StateRepository>()),
      m_active_chain(injector.Get<staking::ActiveChain>()),
      m_stake_validator(injector.Get<staking::StakeValidator>()),
      m_finalization_reward_logic(injector.Get<proposer::FinalizationRewardLogic>()) {}

}  // namespace esperanza
