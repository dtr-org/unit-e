// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <esperanza/rpcproposer.h>

#include <base58.h>
#include <key/mnemonic/mnemonic.h>
#include <random.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <util.h>
#include <utiltime.h>
#include <validation.h>
#include <wallet/crypter.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>
#include <wallet/walletutil.h>

#include <stdint.h>
#include <univalue.h>

#include <ctime>
#include <functional>

void RegisterProposerRPCCommands(CRPCTable &t) {



}


// clang-format off
static const CRPCCommand commands[] = {
//  category               name                      actor (function)         argNames
//  ---------------------  ------------------------  -----------------------  ------------------------------------------
    { "mnemonic",          "mnemonic",               &mnemonic,               {"subcommand", "mnemonic", "passphrase"}},
    { "hidden",            "listreservekeys",        &listreservekeys,        {}},
    { "wallet",            "importmasterkey",        &importmasterkey,        {"seed", "passphrase", "rescan"}},
};
// clang-format on

void RegisterMnemonicRPCCommands(CRPCTable &t) {
  for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++) {
    t.appendCommand(commands[vcidx].name, &commands[vcidx]);
  }
}
