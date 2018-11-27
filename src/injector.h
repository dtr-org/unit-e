// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_INJECTOR_H
#define UNIT_E_INJECTOR_H

#include <dependency_injector.h>

#include <proposer/chainstate.h>
#include <proposer/network.h>
#include <proposer/transactionpicker.h>
#include <staking/block_validator.h>
#include <util.h>

#ifdef ENABLE_WALLET
#include <proposer/blockproposer.h>
#include <proposer/multiwallet.h>
#include <proposer/proposer.h>
#include <proposer/proposer_rpc.h>
#include <proposer/proposer_settings.h>
#endif

class UnitEInjector : public Injector<UnitEInjector> {

  COMPONENT(ArgsManager, Ptr<ArgsManager>, [] { return MakeUnique<Ptr<ArgsManager>>(&gArgs); })

  COMPONENT(Network, proposer::Network, proposer::Network::New)

  COMPONENT(ChainState, proposer::ChainState, proposer::ChainState::New)

  COMPONENT(BlockValidator, staking::BlockValidator, staking::BlockValidator::New)

  COMPONENT(TransactionPicker, proposer::TransactionPicker, proposer::TransactionPicker::New)

#ifdef ENABLE_WALLET

  COMPONENT(MultiWallet, proposer::MultiWallet, proposer::MultiWallet::New);

  COMPONENT(BlockProposer, proposer::BlockProposer, proposer::BlockProposer::New,
            proposer::ChainState,
            proposer::TransactionPicker)

  COMPONENT(ProposerSettings, proposer::Settings, proposer::Settings::New,
            Ptr<ArgsManager>)

  COMPONENT(ProposerRPC, proposer::ProposerRPC, proposer::ProposerRPC::New,
            proposer::ChainState,
            proposer::Network,
            proposer::MultiWallet,
            proposer::Proposer)

  COMPONENT(Proposer, proposer::Proposer, proposer::Proposer::New,
            proposer::Settings,
            proposer::MultiWallet,
            proposer::Network,
            proposer::ChainState,
            proposer::BlockProposer)

#endif

};

#endif  // UNIT_E_INJECTOR_H
