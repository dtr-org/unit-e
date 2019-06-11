// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_INJECTOR_H
#define UNITE_INJECTOR_H

#include <dependency_injector.h>

#include <blockchain/blockchain_behavior.h>
#include <blockchain/blockchain_parameters.h>
#include <blockchain/blockchain_rpc.h>
#include <blockdb.h>
#include <finalization/params.h>
#include <finalization/state_db.h>
#include <finalization/state_processor.h>
#include <finalization/state_repository.h>
#include <injector_config.h>
#include <p2p/finalizer_commits_handler.h>
#include <p2p/graphene_receiver.h>
#include <p2p/graphene_sender.h>
#include <settings.h>
#include <staking/active_chain.h>
#include <staking/block_index_map.h>
#include <staking/block_reward_validator.h>
#include <staking/block_validator.h>
#include <staking/legacy_validation_interface.h>
#include <staking/network.h>
#include <staking/stake_validator.h>
#include <staking/staking_rpc.h>
#include <staking/transactionpicker.h>
#include <txpool.h>
#include <util.h>

#ifdef ENABLE_WALLET
#include <proposer/block_builder.h>
#include <proposer/multiwallet.h>
#include <proposer/proposer.h>
#include <proposer/proposer_logic.h>
#include <proposer/proposer_rpc.h>
#endif

class UnitEInjector : public Injector<UnitEInjector> {

  UNMANAGED_COMPONENT(ArgsManager, ArgsManager, &gArgs)

  UNMANAGED_COMPONENT(BlockchainBehavior, blockchain::Behavior, &blockchain::Behavior::GetGlobal())

  UNMANAGED_COMPONENT(InjectorConfiguration, UnitEInjectorConfiguration, [](UnitEInjector *i) { return &i->m_config; })

  COMPONENT(Settings, Settings, Settings::New,
            ArgsManager,
            blockchain::Behavior)

  COMPONENT(BlockchainRPC, blockchain::BlockchainRPC, blockchain::BlockchainRPC::New,
            blockchain::Behavior)

  COMPONENT(Network, staking::Network, staking::Network::New)

  COMPONENT(BlockIndexMap, staking::BlockIndexMap, staking::BlockIndexMap::New)

  COMPONENT(ActiveChain, staking::ActiveChain, staking::ActiveChain::New)

  COMPONENT(StakeValidator, staking::StakeValidator, staking::StakeValidator::New,
            blockchain::Behavior,
            staking::ActiveChain)

  COMPONENT(BlockValidator, staking::BlockValidator, staking::BlockValidator::New,
            blockchain::Behavior)

  COMPONENT(LegacyValidationInterface, staking::LegacyValidationInterface, staking::LegacyValidationInterface::LegacyImpl,
            staking::ActiveChain,
            staking::BlockValidator,
            staking::Network)

  COMPONENT(BlockRewardValidator, staking::BlockRewardValidator, staking::BlockRewardValidator::New,
            blockchain::Behavior)

  COMPONENT(BlockDB, BlockDB, BlockDB::New)

  COMPONENT(FinalizationParams, finalization::Params, finalization::Params::New,
            UnitEInjectorConfiguration,
            ArgsManager)

  COMPONENT(FinalizationStateDB, finalization::StateDB, finalization::StateDB::New,
            UnitEInjectorConfiguration,
            Settings,
            finalization::Params,
            staking::BlockIndexMap,
            staking::ActiveChain,
            ArgsManager)

  COMPONENT(FinalizationStateRepository, finalization::StateRepository, finalization::StateRepository::New,
            finalization::Params,
            staking::BlockIndexMap,
            staking::ActiveChain,
            finalization::StateDB,
            BlockDB)

  COMPONENT(FinalizationStateProcessor, finalization::StateProcessor, finalization::StateProcessor::New,
            finalization::Params,
            finalization::StateRepository,
            staking::ActiveChain)

  COMPONENT(FinalizerCommitsHandler, p2p::FinalizerCommitsHandler, p2p::FinalizerCommitsHandler::New,
            staking::ActiveChain,
            finalization::StateRepository,
            finalization::StateProcessor)

  COMPONENT(StakingRPC, staking::StakingRPC, staking::StakingRPC::New,
            staking::ActiveChain,
            BlockDB)

  COMPONENT(TxPool, TxPool, TxPool::New);

  COMPONENT(GrapheneReceiver, p2p::GrapheneReceiver, p2p::GrapheneReceiver::New,
            ArgsManager,
            TxPool);

  COMPONENT(GrapheneSender, p2p::GrapheneSender, p2p::GrapheneSender::New,
            ArgsManager,
            TxPool);

#ifdef ENABLE_WALLET

  COMPONENT(TransactionPicker, staking::TransactionPicker, staking::TransactionPicker::New)

  COMPONENT(MultiWallet, proposer::MultiWallet, proposer::MultiWallet::New)

  COMPONENT(BlockBuilder, proposer::BlockBuilder, proposer::BlockBuilder::New,
            Settings)

  COMPONENT(ProposerRPC, proposer::ProposerRPC, proposer::ProposerRPC::New,
            Settings,
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

  UnitEInjectorConfiguration m_config;

 public:
  explicit UnitEInjector(UnitEInjectorConfiguration config) : m_config(config) {}

  //! \brief Initializes a globally available instance of the injector.
  static void Init(UnitEInjectorConfiguration = UnitEInjectorConfiguration{});

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

#endif  // UNITE_INJECTOR_H
