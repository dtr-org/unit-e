// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_INJECTOR_H
#define UNIT_E_INJECTOR_H

#include <dependency_injector.h>

#include <proposer/blockproposer.h>
#include <proposer/chainstate.h>
#include <proposer/multiwallet.h>
#include <proposer/network.h>
#include <proposer/proposer.h>
#include <proposer/proposer_settings.h>
#include <proposer/transactionpicker.h>

class UnitEInjector : public Injector<UnitEInjector> {

  COMPONENT(Network, proposer::Network, proposer::Network::New)

  COMPONENT(ChainState, proposer::ChainState, proposer::ChainState::New)

  COMPONENT(MultiWallet, proposer::MultiWallet,
            proposer::MultiWallet::New);

  COMPONENT(TransactionPicker, proposer::TransactionPicker,
            proposer::TransactionPicker::MakeBlockAssemblerAdapter)

  COMPONENT(BlockProposer, proposer::BlockProposer,
            proposer::BlockProposer::New, proposer::ChainState,
            proposer::TransactionPicker)

  COMPONENT(ProposerSettings, proposer::Settings,
            proposer::Settings::New)

  COMPONENT(Proposer, proposer::Proposer, proposer::Proposer::New,
            proposer::Settings, proposer::MultiWallet, proposer::Network,
            proposer::ChainState, proposer::BlockProposer)
};

#endif  // UNIT_E_INJECTOR_H
