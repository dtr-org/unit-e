// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_ESPERANZA_WALLETEXTENSION_DEPS_H
#define UNIT_E_ESPERANZA_WALLETEXTENSION_DEPS_H

#include <dependency.h>
#include <settings.h>
#include <staking/stake_validator.h>

class UnitEInjector;

namespace esperanza {

struct WalletExtensionDeps {
  Dependency<Settings> settings;
  Dependency<staking::StakeValidator> m_stake_validator;

  WalletExtensionDeps() noexcept;
  explicit WalletExtensionDeps(const UnitEInjector &) noexcept;

  //! \brief Constructor for testing only.
  //!
  //! \param settings A pointer to mocked stake validator.
  WalletExtensionDeps(Dependency<staking::StakeValidator> stake_validator) noexcept;

  staking::StakeValidator &GetStakeValidator() const {
    assert(m_stake_validator != nullptr &&
           "staking::StakeValidator not available: test-only wallet extension used in production");
    return *m_stake_validator;
  }
};

}  // namespace esperanza

#endif  //UNIT_E_WALLETEXTENSION_DEPS_H
