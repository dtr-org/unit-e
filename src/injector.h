// Copyright (c) 2018 The unit-e core developers
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

  COMPONENT(Network, proposer::Network, proposer::Network::MakeNetwork)

  COMPONENT(ChainState, proposer::ChainState, proposer::ChainState::MakeChain)

  COMPONENT(MultiWallet, proposer::MultiWallet,
            proposer::MultiWallet::MakeMultiWallet);

  COMPONENT(TransactionPicker, proposer::TransactionPicker,
            proposer::TransactionPicker::MakeBlockAssemblerAdapter)

  COMPONENT(BlockProposer, proposer::BlockProposer,
            proposer::BlockProposer::MakeBlockProposer, proposer::ChainState,
            proposer::TransactionPicker)

  COMPONENT(ProposerSettings, proposer::Settings,
            proposer::Settings::MakeSettings)

  COMPONENT(Proposer, proposer::Proposer, proposer::Proposer::MakeProposer,
            proposer::Settings, proposer::MultiWallet, proposer::Network,
            proposer::ChainState, proposer::BlockProposer)
};

#endif  // UNIT_E_INJECTOR_H
