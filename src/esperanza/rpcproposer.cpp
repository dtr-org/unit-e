// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/rpcproposer.h>

#include <core_io.h>
#include <esperanza/proposer.h>
#include <esperanza/proposer_init.h>
#include <net.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <utiltime.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <stdint.h>
#include <univalue.h>

    static UniValue
    GetWalletInfo(const std::vector<CWalletRef> &wallets) {
  UniValue result(UniValue::VARR);
  for (const auto &wallet : wallets) {
    const auto &walletExt = wallet->GetWalletExtension();
    const auto &proposerState = walletExt.GetProposerState();
    UniValue info(UniValue::VOBJ);
    info.pushKV("wallet", UniValue(wallet->GetName()));
    info.pushKV("balance", ValueFromAmount(wallet->GetBalance()));
    info.pushKV("stakeable_balance",
                ValueFromAmount(walletExt.GetStakeableBalance()));
    info.pushKV("status", UniValue(proposerState.m_status._to_string()));
    result.push_back(info);
  }
  return result;
}

UniValue proposerstatus(const JSONRPCRequest &request) {
  UniValue result(UniValue::VOBJ);
  result.pushKV("wallets", GetWalletInfo(vpwallets));
  const auto syncStatus = GetInitialBlockDownloadStatus();
  result.pushKV("sync_status", UniValue(syncStatus._to_string()));
  result.pushKV("time", DateTimeToString(GetTime()));
  const uint64_t cin = g_connman->GetNodeCount(CConnman::CONNECTIONS_IN);
  const uint64_t cout = g_connman->GetNodeCount(CConnman::CONNECTIONS_OUT);
  result.pushKV("incoming_connections", UniValue(cin));
  result.pushKV("outgoing_connections", UniValue(cout));
  return result;
}

UniValue proposerwake(const JSONRPCRequest &request) {
  esperanza::WakeProposer();
  return proposerstatus(request);
}

// clang-format off
static const CRPCCommand commands[] = {
//  category               name                      actor (function)         argNames
//  ---------------------  ------------------------  -----------------------  ------------------------------------------
    { "esperanza",         "proposerstatus",         &proposerstatus,         {}},
    { "esperanza",         "proposerwake",           &proposerwake,           {}},
};
// clang-format on

void RegisterProposerRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
