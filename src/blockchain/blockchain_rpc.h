// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_BLOCKCHAIN_BLOCKCHAIN_RPC_H
#define UNITE_BLOCKCHAIN_BLOCKCHAIN_RPC_H

#include <blockchain/blockchain_behavior.h>
#include <dependency.h>
#include <rpc/server.h>

#include <univalue.h>

#include <memory>

namespace blockchain {

class BlockchainRPC {

 public:
  virtual UniValue getchainparams(const JSONRPCRequest &request) const = 0;

  virtual ~BlockchainRPC() = default;

  static std::unique_ptr<BlockchainRPC> New(Dependency<blockchain::Behavior>);
};

}  // namespace blockchain

#endif  // UNITE_BLOCKCHAIN_BLOCKCHAIN_RPC_H
