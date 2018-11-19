// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/proposer.h>

#include <core_io.h>
#include <proposer/proposer.h>
#include <proposer/proposer_init.h>
#include <net.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <utiltime.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <stdint.h>
#include <univalue.h>

static UniValue GetWalletInfo(const std::vector<CWalletRef> &wallets) {
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
    info.pushKV("searches", UniValue(proposerState.m_numSearches));
    info.pushKV("searches_attempted",
                UniValue(proposerState.m_numSearchAttempts));
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
  proposer::WakeProposer();
  return proposerstatus(request);
}

// clang-format off
static const CRPCCommand commands[] = {
//  category               name                      actor (function)         argNames
//  ---------------------  ------------------------  -----------------------  ------------------------------------------
    { "proposer",         "proposerstatus",         &proposerstatus,         {}},
    { "proposer",         "proposerwake",           &proposerwake,           {}},
};
// clang-format on

void RegisterProposerRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
