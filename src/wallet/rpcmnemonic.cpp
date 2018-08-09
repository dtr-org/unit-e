// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <base58.h>
#include <key.h>
#include <pubkey.h>
#include <utilstrencodings.h>
#include <key/mnemonic.h>
#include <wallet/rpcmnemonic.h>

#include <tinyformat.h>

UniValue mnemonicinfo(const JSONRPCRequest& request)
{
    std::string mnemonic = "";
    if (request.params.size() > 0) {
        mnemonic = request.params[0].get_str();
    }
    std::string passphrase = "";
    if (request.params.size() > 1) {
        passphrase = request.params[1].get_str();
    }
    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
                "mnemonicinfo\n"
                "\nShow information a Private Extended Key form a Mnemonic seed according to BIP39."
                "\nShows the mnemonics language, the seed in hex, and the root key as base58 private key.\n"
                "\nArguments:\n"
                "1. \"seed\"       (string, required) a list of words to create the master key from\n"
                "2. \"passphrase\" (string, optional) an optional passphrase to protect the key\n"
                "\nExamples:\n"
                + HelpExampleCli("setmasterkey", "\"next debate force grief bleak want truck prepare theme lecture wear century rich grace someone\"")
                + HelpExampleRpc("setmasterkey", "\"next debate force grief bleak want truck prepare theme lecture wear century rich grace someone\"")
        );
    }

    key::mnemonic::Seed seed(mnemonic, passphrase);

    UniValue response(UniValue::VOBJ);

    response.pushKV("language", UniValue(seed.GetHumandReadableLanguage()));
    response.pushKV("language_tag", UniValue(seed.GetLanguageTag()));
    response.pushKV("bip39_seed", UniValue(seed.GetHexSeed()));
    response.pushKV("bip32_root", UniValue(seed.GetExtKey58().ToString()));

    return response;
}

static const CRPCCommand commands[] =
{ //  category              name                        actor (function)           argNames
  //  --------------------- --------------------------- -------------------------- --------

    { "mnemonic",           "mnemonicinfo",             &mnemonicinfo,             {"seed", "passphrase"} },
};

void RegisterMnemonicRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++) {
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}
