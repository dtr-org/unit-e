// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_INJECTOR_H
#define UNIT_E_INJECTOR_H

#include <dependency_injector.h>

#include <staking/block_validator.h>
#include <staking/chainstate.h>
#include <staking/network.h>
#include <staking/transactionpicker.h>
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

  COMPONENT(Network, staking::Network, staking::Network::New)

  COMPONENT(ChainState, staking::ChainState, staking::ChainState::New)

  COMPONENT(BlockValidator, staking::BlockValidator, staking::BlockValidator::New)

  COMPONENT(TransactionPicker, staking::TransactionPicker, staking::TransactionPicker::New)

#ifdef ENABLE_WALLET

  COMPONENT(MultiWallet, proposer::MultiWallet, proposer::MultiWallet::New);

  COMPONENT(BlockProposer, proposer::BlockProposer, proposer::BlockProposer::New,
            staking::ChainState,
            staking::TransactionPicker)

  COMPONENT(ProposerSettings, proposer::Settings, proposer::Settings::New,
            Ptr<ArgsManager>)

  COMPONENT(ProposerRPC, proposer::ProposerRPC, proposer::ProposerRPC::New,
            staking::ChainState,
            staking::Network,
            proposer::MultiWallet,
            proposer::Proposer)

  COMPONENT(Proposer, proposer::Proposer, proposer::Proposer::New,
            proposer::Settings,
            proposer::MultiWallet,
            staking::Network,
            staking::ChainState,
            proposer::BlockProposer)

#endif
};

#endif  // UNIT_E_INJECTOR_H
