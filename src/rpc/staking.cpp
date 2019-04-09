// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/staking.h>

#include <injector.h>
#include <rpc/server.h>
#include <staking/staking_rpc.h>

#include <univalue.h>

#define STAKING_RPC_COMMAND(NAME, ...)                                  \
  static CRPCCommand NAME = {                                           \
      "staking", #NAME, [](const JSONRPCRequest &request) -> UniValue { \
        return GetComponent<staking::StakingRPC>()->NAME(request);      \
      },                                                                \
      {__VA_ARGS__}};                                                   \
  t.appendCommand(NAME.name, &NAME);

void RegisterStakingRPCCommands(CRPCTable &t) {
  STAKING_RPC_COMMAND(tracechain, "start", "length");
  STAKING_RPC_COMMAND(tracestake, "start", "length", "reverse");
}
