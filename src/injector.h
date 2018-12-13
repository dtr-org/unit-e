// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_INJECTOR_H
#define UNIT_E_INJECTOR_H

#include <dependency_injector.h>

#include <blockchain/blockchain_behavior.h>
#include <blockchain/blockchain_parameters.h>
#include <staking/active_chain.h>
#include <staking/block_validator.h>
#include <staking/network.h>
#include <staking/stake_validator.h>
#include <staking/transactionpicker.h>
#include <util.h>
#include <validation.h>

#ifdef ENABLE_WALLET
#include <proposer/multiwallet.h>
#include <proposer/proposer.h>
#include <proposer/proposer_rpc.h>
#include <proposer/proposer_settings.h>
#endif

class UnitEInjector : public Injector<UnitEInjector> {

  UNMANAGED_COMPONENT(ArgsManager, ::ArgsManager, &gArgs)

  COMPONENT(BlockchainBehavior, blockchain::Behavior, blockchain::Behavior::New,
            ::ArgsManager)

  COMPONENT(Network, staking::Network, staking::Network::New)

  COMPONENT(ActiveChain, staking::ActiveChain, staking::ActiveChain::New,
            blockchain::Behavior)

  COMPONENT(StakeValidator, staking::StakeValidator, staking::StakeValidator::New,
            blockchain::Behavior)

  COMPONENT(BlockValidator, staking::BlockValidator, staking::BlockValidator::New,
            blockchain::Behavior)

  COMPONENT(TransactionPicker, staking::TransactionPicker, staking::TransactionPicker::New)

#ifdef ENABLE_WALLET

  COMPONENT(MultiWallet, proposer::MultiWallet, proposer::MultiWallet::New);

  COMPONENT(ProposerSettings, proposer::Settings, proposer::Settings::New,
            ArgsManager)

  COMPONENT(ProposerRPC, proposer::ProposerRPC, proposer::ProposerRPC::New,
            proposer::MultiWallet,
            staking::Network,
            staking::ActiveChain,
            proposer::Proposer)

  COMPONENT(Proposer, proposer::Proposer, proposer::Proposer::New,
            proposer::Settings,
            proposer::MultiWallet,
            staking::Network,
            staking::ActiveChain)

#endif
};

#endif  // UNIT_E_INJECTOR_H
