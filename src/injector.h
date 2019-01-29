// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_INJECTOR_H
#define UNIT_E_INJECTOR_H

#include <dependency_injector.h>

#include <blockchain/blockchain_behavior.h>
#include <blockchain/blockchain_parameters.h>
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
            ::ArgsManager)

  COMPONENT(Network, staking::Network, staking::Network::New)

  COMPONENT(ActiveChain, staking::ActiveChain, staking::ActiveChain::New,
            blockchain::Behavior)

  COMPONENT(StakeValidator, staking::StakeValidator, staking::StakeValidator::New,
            blockchain::Behavior)

  COMPONENT(BlockValidator, staking::BlockValidator, staking::BlockValidator::New,
            blockchain::Behavior)

  COMPONENT(TransactionPicker, staking::TransactionPicker, staking::TransactionPicker::New)

  COMPONENT(BlockDB, ::BlockDB, BlockDB::New)

#ifdef ENABLE_WALLET

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
};

#endif  // UNIT_E_INJECTOR_H
