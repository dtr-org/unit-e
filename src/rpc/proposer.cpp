// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/proposer.h>

#include <proposer/proposer.h>
#include <rpc/server.h>

#include <univalue.h>

namespace {
Dependency<proposer::ProposerRPC> proposerRPC;
UniValue NotAvailable() {
  return "proposer is not available yet";
}
}  // namespace

void SetProposerRPC(Dependency<proposer::ProposerRPC> rpc) {
  proposerRPC = rpc;
}

#define PROPOSER_RPC_COMMAND(NAME, ARG_NAMES...)                            \
  {                                                                         \
    static CRPCCommand NAME = {                                             \
        "proposer", #NAME, [](const JSONRPCRequest &request) -> UniValue {  \
          return proposerRPC ? proposerRPC->NAME(request) : NotAvailable(); \
        },                                                                  \
        {ARG_NAMES}};                                                       \
    t.appendCommand(NAME.name, &NAME);                                      \
  }

void RegisterProposerRPCCommands(CRPCTable &t) {
  PROPOSER_RPC_COMMAND(proposerstatus);
  PROPOSER_RPC_COMMAND(proposerwake);
}
