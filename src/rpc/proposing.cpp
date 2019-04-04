// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/proposing.h>

#include <injector.h>
#include <proposer/proposer.h>
#include <rpc/server.h>

#include <univalue.h>

#define PROPOSER_RPC_COMMAND(NAME, ARG_NAMES...)                          \
  static CRPCCommand NAME = {                                             \
      "proposing", #NAME, [](const JSONRPCRequest &request) -> UniValue { \
        return GetComponent<proposer::ProposerRPC>()->NAME(request);      \
      },                                                                  \
      {ARG_NAMES}};                                                       \
  t.appendCommand(NAME.name, &NAME);

void RegisterProposerRPCCommands(CRPCTable &t) {
  PROPOSER_RPC_COMMAND(getstakeablecoins);
  PROPOSER_RPC_COMMAND(proposerstatus);
  PROPOSER_RPC_COMMAND(proposerwake);
}
