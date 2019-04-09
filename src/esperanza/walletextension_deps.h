// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_ESPERANZA_WALLETEXTENSION_DEPS_H
#define UNIT_E_ESPERANZA_WALLETEXTENSION_DEPS_H

#include <dependency.h>
#include <finalization/state_repository.h>
#include <settings.h>
#include <staking/active_chain.h>

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
  //!
  //! \param settings A pointer to mmocked test settings.
  WalletExtensionDeps(Dependency<::Settings> settings) noexcept;

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
};

}  // namespace esperanza

#endif  //UNIT_E_WALLETEXTENSION_DEPS_H
