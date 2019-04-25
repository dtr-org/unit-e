// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_ESPERANZA_WALLETEXTENSION_DEPS_H
#define UNIT_E_ESPERANZA_WALLETEXTENSION_DEPS_H

#include <dependency.h>
#include <finalization/state_repository.h>
#include <proposer/finalization_reward_logic.h>
#include <settings.h>
#include <staking/active_chain.h>
#include <staking/stake_validator.h>

class UnitEInjector;

namespace esperanza {

//! \brief Dependencies of the Wallet Extension
//!
//! New components in unit-e are typically written in a proper
//! component-oriented style with defined dependencies which are
//! injected via the constructor. The wallet, of which there are
//! possibly many instances, is not created in the uniform same way
//! as components are typically. Thus the dependencies are extracted
//! into this class which itself takes them from an injected
//! constructor, to highlight the special case of the WalletExtension.
class WalletExtensionDeps {

 private:
  const Dependency<Settings> m_settings;
  const Dependency<finalization::StateRepository> m_finalization_state_repository;
  const Dependency<staking::ActiveChain> m_active_chain;
  const Dependency<staking::StakeValidator> m_stake_validator;
  const Dependency<proposer::FinalizationRewardLogic> m_finalization_reward_logic;

 public:
  //! \brief Constructor for testing only.
  //!
  //! This constructor merely exists because there is a no-args
  //! constructor on CWallet too. That constructor is used and
  //! supposed to be used in unit tests only.
  WalletExtensionDeps() noexcept;

  //! \brief Constructor for testing only.
  //!
  //! This constructor is used in WalletTestingSetup and the
  //! Fixture in proposer_tests.
  WalletExtensionDeps(
      Dependency<Settings> settings,
      Dependency<staking::StakeValidator> stake_validator,
      Dependency<proposer::FinalizationRewardLogic> finalization_reward_logic) noexcept;

  //! \brief Proper constructor for production use.
  //!
  //! Retrieves the dependencies of the wallet from an injected
  //! injector.
  explicit WalletExtensionDeps(const UnitEInjector &) noexcept;

  Settings &GetSettings() const {
    return *m_settings;
  }

  finalization::StateRepository &GetFinalizationStateRepository() const {
    assert(m_finalization_state_repository != nullptr &&
           "finalization::StateRepository not available: test-only wallet extension used in production, see comments in walletextension_deps.h");
    return *m_finalization_state_repository;
  }

  staking::ActiveChain &GetActiveChain() const {
    assert(m_active_chain != nullptr &&
           "staking::ActiveChain not available: test-only wallet extension used in production, see comments in walletextension_deps.h");
    return *m_active_chain;
  }

  staking::StakeValidator &GetStakeValidator() const {
    assert(m_stake_validator != nullptr &&
           "staking::StakeValidator not available: test-only wallet extension used in production, see comments in walletextension_deps.h");
    return *m_stake_validator;
  }

  proposer::FinalizationRewardLogic &GetFinalizationRewardLogic() const {
    assert(m_finalization_reward_logic != nullptr &&
           "proposer::FinalizationRewardLogic not available: test-only wallet extension used in production, see comments in walletextension_deps.h");
    return *m_finalization_reward_logic;
  }
};

}  // namespace esperanza

#endif  //UNIT_E_WALLETEXTENSION_DEPS_H
