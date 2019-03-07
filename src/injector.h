// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_INJECTOR_H
#define UNIT_E_INJECTOR_H

#include <dependency_injector.h>

#include <blockchain/blockchain_behavior.h>
#include <blockchain/blockchain_parameters.h>
#include <blockchain/blockchain_rpc.h>
#include <finalization/state_processor.h>
#include <finalization/state_repository.h>
#include <p2p/finalizer_commits.h>
#include <settings.h>
#include <staking/active_chain.h>
#include <staking/block_validator.h>
#include <staking/network.h>
#include <staking/stake_validator.h>
#include <staking/transactionpicker.h>
#include <blockdb.h>
#include <util.h>
#include <validation.h>

#ifdef ENABLE_WALLET
#include <proposer/block_builder.h>
#include <proposer/multiwallet.h>
#include <proposer/proposer.h>
#include <proposer/proposer_rpc.h>
#endif

class UnitEInjector : public Injector<UnitEInjector> {

  UNMANAGED_COMPONENT(ArgsManager, ::ArgsManager, &gArgs)

  UNMANAGED_COMPONENT(BlockchainBehavior, blockchain::Behavior, &blockchain::Behavior::GetGlobal())

  COMPONENT(Settings, ::Settings, Settings::New,
            ::ArgsManager,
            blockchain::Behavior)

  COMPONENT(BlockchainRPC, blockchain::BlockchainRPC, blockchain::BlockchainRPC::New,
            blockchain::Behavior)

  COMPONENT(Network, staking::Network, staking::Network::New)

  COMPONENT(ActiveChain, staking::ActiveChain, staking::ActiveChain::New)

  COMPONENT(StakeValidator, staking::StakeValidator, staking::StakeValidator::New,
            blockchain::Behavior,
            staking::ActiveChain)

  COMPONENT(BlockValidator, staking::BlockValidator, staking::BlockValidator::New,
            blockchain::Behavior)

  COMPONENT(BlockDB, ::BlockDB, BlockDB::New)

  COMPONENT(FinalizationStateRepository, finalization::StateRepository, finalization::StateRepository::New,
            staking::ActiveChain)

  COMPONENT(FinalizationStateProcessor, finalization::StateProcessor, finalization::StateProcessor::New,
            finalization::StateRepository,
            staking::ActiveChain)

  COMPONENT(FinalizerCommits, p2p::FinalizerCommits, p2p::FinalizerCommits::New,
            staking::ActiveChain,
            finalization::StateRepository,
            finalization::StateProcessor)

#ifdef ENABLE_WALLET

  COMPONENT(TransactionPicker, staking::TransactionPicker, staking::TransactionPicker::New)

  COMPONENT(MultiWallet, proposer::MultiWallet, proposer::MultiWallet::New)

  COMPONENT(BlockBuilder, proposer::BlockBuilder, proposer::BlockBuilder::New,
            blockchain::Behavior,
            Settings)

  COMPONENT(ProposerRPC, proposer::ProposerRPC, proposer::ProposerRPC::New,
            proposer::MultiWallet,
            staking::Network,
            staking::ActiveChain,
            proposer::Proposer)

  COMPONENT(ProposerLogic, proposer::Logic, proposer::Logic::New,
            blockchain::Behavior,
            staking::Network,
            staking::ActiveChain,
            staking::StakeValidator)

  COMPONENT(Proposer, proposer::Proposer, proposer::Proposer::New,
            Settings,
            blockchain::Behavior,
            proposer::MultiWallet,
            staking::Network,
            staking::ActiveChain,
            staking::TransactionPicker,
            proposer::BlockBuilder,
            proposer::Logic)

#endif

 public:
  //! \brief Initializes a globally available instance of the injector.
  static void Init();

  //! \brief Destructs the injector and all components managed by it.
  static void Destroy();

  template <typename T>
  Dependency<T> Get() const {
    // the passed nullptr merely serves to select the right getter
    return Get(static_cast<T *>(nullptr));
  }
};

//! \brief Retrieves the globally available instance of the injector.
//!
//! This mechanism solely exists such that old bitcoin code which is not
//! part of the component framework can access components. It must never
//! be invoked from within any function that lives in a component.
//!
//! It is actually an instance of the Service Locator pattern, which is
//! considered an anti-pattern (by the author of this comment), but a
//! necessary evil to interface legacy code with the component based design.
UnitEInjector &GetInjector();

template <typename T>
Dependency<T> GetComponent() {
  return GetInjector().Get<T>();
}

#endif  // UNIT_E_INJECTOR_H
