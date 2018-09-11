// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/rpcproposer.h>

#include <rpc/server.h>
#include <rpc/util.h>
#include <util.h>
#include <utiltime.h>
#include <wallet/wallet.h>

#include <stdint.h>
#include <univalue.h>

#include <ctime>
#include <functional>

UniValue proposerstatus(const JSONRPCRequest &request) {
  UniValue result(UniValue::VARR);
  for (const auto &wallet : vpwallets) {
    const auto &walletExt = wallet->GetWalletExtension();
    const auto &proposerState = walletExt.GetProposerState();
    UniValue walletResult(UniValue::VOBJ);
    walletResult.pushKV("wallet", UniValue(wallet->GetName()));
    walletResult.pushKV("status", UniValue(proposerState.m_status._to_string()));
    result.push_back(walletResult);
  }
  return result;
}

// clang-format off
static const CRPCCommand commands[] = {
//  category               name                      actor (function)         argNames
//  ---------------------  ------------------------  -----------------------  ------------------------------------------
    { "proposer",          "proposerstatus",         &proposerstatus,         {}},
};
// clang-format on

void RegisterProposerRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
