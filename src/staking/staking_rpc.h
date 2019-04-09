// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STAKING_RPC_H
#define UNIT_E_STAKING_RPC_H

#include <dependency.h>
#include <rpc/server.h>

#include <univalue.h>

class BlockDB;

namespace staking {

class ActiveChain;

//! \brief The staking RPC commands, dependency injected.
//!
//! Usually RPC commands are statically bound by referencing function pointers.
//! For the staking RPC commands to be part of the dependency injector a
//! proper module is defined and the commands are bound slightly differently
//! (see rpc/staking.cpp).
//!
//! This class is an interface.
class StakingRPC {

 public:
  virtual UniValue tracechain(const JSONRPCRequest &request) = 0;

  virtual UniValue tracestake(const JSONRPCRequest &request) = 0;

  virtual ~StakingRPC() = default;

  static std::unique_ptr<StakingRPC> New(
      Dependency<staking::ActiveChain>,
      Dependency<::BlockDB>);
};

}  // namespace staking

#endif  //UNIT_E_PROPOSER_RPC_H
