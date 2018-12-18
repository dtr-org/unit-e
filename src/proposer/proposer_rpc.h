// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_RPC_H
#define UNIT_E_PROPOSER_RPC_H

#include <dependency.h>
#include <proposer/multiwallet.h>
#include <proposer/proposer.h>
#include <rpc/server.h>
#include <staking/active_chain.h>
#include <staking/network.h>

#include <univalue.h>

namespace proposer {

//! \brief The proposer RPC commands, dependency injected.
//!
//! Usually RPC commands are statically bound by referencing function pointers.
//! For the proposer RPC commands to be part of the dependency injector a
//! proper module is defined and the commands are bound slightly differently
//! (see rpc/proposer.cpp).
//!
//! This class is an interface.
class ProposerRPC {

 public:
  virtual UniValue proposerstatus(const JSONRPCRequest &request) = 0;

  virtual UniValue proposerwake(const JSONRPCRequest &request) = 0;

  virtual ~ProposerRPC() = default;

  static std::unique_ptr<ProposerRPC> New(
      Dependency<MultiWallet>,
      Dependency<staking::Network>,
      Dependency<staking::ActiveChain>,
      Dependency<Proposer>);
};

}  // namespace proposer

#endif  //UNIT_E_PROPOSER_RPC_H
