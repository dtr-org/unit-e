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
#include <staking/transactionpicker.h>
#include <util.h>
#include <validation.h>

#ifdef ENABLE_WALLET
#include <proposer/blockproposer.h>
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

  COMPONENT(ChainState, staking::ActiveChain, staking::ActiveChain::New,
            blockchain::Behavior)

  COMPONENT(BlockValidator, staking::BlockValidator, staking::BlockValidator::New)

  COMPONENT(TransactionPicker, staking::TransactionPicker, staking::TransactionPicker::New)

#ifdef ENABLE_WALLET

  COMPONENT(MultiWallet, proposer::MultiWallet, proposer::MultiWallet::New);

  COMPONENT(BlockProposer, proposer::BlockProposer, proposer::BlockProposer::New,
            staking::ActiveChain,
            staking::TransactionPicker)

  COMPONENT(ProposerSettings, proposer::Settings, proposer::Settings::New,
            ArgsManager)

  COMPONENT(ProposerRPC, proposer::ProposerRPC, proposer::ProposerRPC::New,
            staking::ActiveChain,
            staking::Network,
            proposer::MultiWallet,
            proposer::Proposer)

  COMPONENT(Proposer, proposer::Proposer, proposer::Proposer::New,
            proposer::Settings,
            proposer::MultiWallet,
            staking::Network,
            staking::ActiveChain,
            proposer::BlockProposer)

#endif
};

#endif  // UNIT_E_INJECTOR_H
