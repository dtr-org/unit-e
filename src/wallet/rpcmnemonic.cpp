// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <base58.h>
#include <key/mnemonic/mnemonic.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <util.h>
#include <wallet/crypter.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>
#include <wallet/walletutil.h>

#include <stdint.h>
#include <univalue.h>

#include <ctime>
#include <functional>

UniValue mnemonicinfo(const JSONRPCRequest& request) {
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
        "\nShow information a BIP39 mnemonic seed "
        "according to BIP39."
        "\nShows the mnemonics language, the seed in hex, and the root key as "
        "base58 private key.\n"
        "\nArguments:\n"
        "1. \"seed\"       (string, required) a list of words to create the "
        "master key from\n"
        "2. \"passphrase\" (string, optional) an optional passphrase to "
        "protect the key\n"
        "\nExamples:\n" +
        HelpExampleCli("mnemonicinfo",
                       "\"next debate force grief bleak want truck prepare "
                       "theme lecture wear century rich grace someone\"") +
        HelpExampleRpc("mnemonicinfo",
                       "\"next debate force grief bleak want truck prepare "
                       "theme lecture wear century rich grace someone\""));
  }
  key::mnemonic::Seed seed(mnemonic, passphrase);
  UniValue response(UniValue::VOBJ);
  response.pushKV("language", UniValue(seed.GetHumandReadableLanguage()));
  response.pushKV("language_tag", UniValue(seed.GetLanguageTag()));
  response.pushKV("bip39_seed", UniValue(seed.GetHexSeed()));
  response.pushKV("bip32_root", UniValue(seed.GetExtKey58().ToString()));
  response.pushKV("entropy", UniValue(seed.GetHexEntropy()));
  return response;
}

UniValue mnemoniclistlanguages(const JSONRPCRequest& request) {
  UniValue response(UniValue::VOBJ);
  for (size_t ix = 0; ix < static_cast<int>(key::mnemonic::Language::COUNT);
       ++ix) {
    const auto language = static_cast<key::mnemonic::Language>(ix);
    response.pushKV(key::mnemonic::GetLanguageTag(language),
                    UniValue(key::mnemonic::GetLanguageDesc(language)));
  }
  return response;
}

UniValue importmasterkey(const JSONRPCRequest& request) {
  if (request.fHelp || request.params.size() > 2 || request.params.size() < 1) {
    throw std::runtime_error(
        "importmasterkey\n"
        "\nImport a master key from a BIP39 seed, with an optional passphrase."
        "\nArguments:\n"
        "1. \"seed\"       (string, required) a list of words to create the "
        "master key from\n"
        "2. \"passphrase\" (string, optional) an optional passphrase to "
        "protect the key\n"
        "\nExamples:\n" +
        HelpExampleCli("importmasterkey",
                       "\"next debate force grief bleak want truck prepare "
                       "theme lecture wear century rich grace someone\"") +
        HelpExampleRpc("importmasterkey",
                       "\"next debate force grief bleak want truck prepare "
                       "theme lecture wear century rich grace someone\""));
  }
  CWallet* const wallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(wallet, request.fHelp)) {
    throw std::runtime_error("no unlocked wallet open!");
  }
  LOCK(wallet->cs_wallet);
  const std::string walletFileName = wallet->GetName();
  std::string mnemonic(request.params[0].get_str());
  std::string passphrase;
  if (request.params.size() > 1) {
    passphrase = request.params[1].get_str();
  }
  key::mnemonic::Seed seed(mnemonic, passphrase);
  std::string error;
  if (!wallet->GetWalletExtension().SetMasterKeyFromSeed(seed, error)) {
    throw std::runtime_error(error);
  }
  UniValue response(UniValue::VOBJ);
  response.pushKV("wallet", UniValue(walletFileName));
  response.pushKV("language", UniValue(seed.GetHumandReadableLanguage()));
  response.pushKV("language_tag", UniValue(seed.GetLanguageTag()));
  response.pushKV("bip39_seed", UniValue(seed.GetHexSeed()));
  response.pushKV("bip32_root", UniValue(seed.GetExtKey58().ToString()));
  response.pushKV("success", UniValue(true));
  return response;
}

/**
 * Internal, for functional tests.
 *
 * Dumps all the reserve keys for verifying the mnemonic seed generates the same
 * wallet deterministically.
 */
UniValue listreservekeys(const JSONRPCRequest& request) {
  CWallet* const wallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(wallet, request.fHelp)) {
    throw std::runtime_error("no unlocked wallet open!");
  }
  const std::map<CKeyID, int64_t> allReserveKeys = wallet->GetAllReserveKeys();
  UniValue reserveKeys(UniValue::VARR);
  for (auto& it : allReserveKeys) {
    const CKeyID keyID = it.first;
    CKey key;
    wallet->GetKey(keyID, key);
    UniValue keyPair(UniValue::VOBJ);
    CPrivKey privKey = key.GetPrivKey();
    CPubKey pubKey = key.GetPubKey();
    keyPair.pushKV("public_key", UniValue(pubKey.GetHash().GetHex()));
    keyPair.pushKV("private_key",
                   UniValue(EncodeBase58(privKey.data(),
                                         privKey.data() + privKey.size())));
    reserveKeys.push_back(keyPair);
  }
  return reserveKeys;
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category               name                      actor (function)         argNames
  //  ---------------------  ------------------------  -----------------------  ----------
  {   "mnemonic",            "mnemonicinfo",           &mnemonicinfo,           {"seed", "passphrase"} },
  {   "mnemonic",            "mnemoniclistlanguages",  &mnemoniclistlanguages,  {} },
  {   "hidden",              "listreservekeys",        &listreservekeys,        {} },
  {   "wallet",              "importmasterkey",        &importmasterkey,        {"seed", "passphrase"} },
};
// clang-format on

void RegisterMnemonicRPCCommands(CRPCTable& t) {
  for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++) {
    t.appendCommand(commands[vcidx].name, &commands[vcidx]);
  }
}
