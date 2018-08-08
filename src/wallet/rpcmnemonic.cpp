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
//    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
//    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
//        return NullUniValue;
//    }
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
//    EnsureWalletIsUnlocked(pwallet);

    int language = key::mnemonic::MnemonicDetectLanguage(mnemonic);
    if (0 == language) {
        throw std::runtime_error("invalid mnemonic: did not detect a known language");
    }

    UniValue response(UniValue::VOBJ);

    std::string error;
    std::vector<uint8_t> seed, entropy;

    if (0 != key::mnemonic::MnemonicDecode(language, mnemonic, entropy, error)) {
        throw std::runtime_error(strprintf("invalid mnemonic: %s", error.c_str()));
    }
    if (0 != key::mnemonic::MnemonicToSeed(mnemonic, passphrase, seed)) {
        // this should never happen as the previous if statement already checks whether the mnemonic can be decoded.
        throw std::runtime_error(strprintf("invalid mnemonic: %s", mnemonic.c_str()));
    }

    std::string hexSeed = EncodeBase16(seed);

    CExtKey masterKey;
    masterKey.SetMaster(seed.data(), seed.size());

    CUnitEExtKey masterKey58;
    masterKey58.SetKey(masterKey);

    response.pushKV("language", UniValue(key::mnemonic::mnLanguagesDesc[language]));
    response.pushKV("language_tag", UniValue(key::mnemonic::mnLanguagesTag[language]));
    response.pushKV("bip39_seed", UniValue(hexSeed));
    response.pushKV("bip32_root", UniValue(masterKey58.ToString()));

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
