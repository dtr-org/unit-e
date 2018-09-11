// Copyright (c) 2015 The ShadowCoin developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

UniValue mnemonicnew(const JSONRPCRequest &request) {
  key::mnemonic::Language language = key::mnemonic::Language::ENGLISH;
  size_t maxTries = 16;
  size_t numEntropyBytes = 32;
  std::string passphrase;

  if (request.params.size() >= 2) {
    passphrase = request.params[1].get_str();
  }

  std::string error;
  std::vector<uint8_t> entropy(numEntropyBytes);
  std::vector<uint8_t> seed;

  std::string mnemonic;
  CExtKey masterKey;

  for (size_t i = 0; i < maxTries; ++i) {
    GetStrongRandBytes(&entropy[0], static_cast<int>(numEntropyBytes));

    if (0 != key::mnemonic::Encode(language, entropy, mnemonic, error)) {
      throw JSONRPCError(
          RPC_INTERNAL_ERROR,
          strprintf("failed to decode mnemonic: %s", error.c_str()));
    }
    if (0 != key::mnemonic::ToSeed(mnemonic, passphrase, seed)) {
      throw JSONRPCError(RPC_INTERNAL_ERROR,
                         strprintf("failed to calculate seed from mnemonic %s",
                                   error.c_str()));
    }
    masterKey.SetMaster(&seed[0], static_cast<unsigned int>(seed.size()));
    if (masterKey.key.IsValid()) {
      break;
    }
  }
  const key::mnemonic::Seed seedInfo(mnemonic, passphrase);
  UniValue response(UniValue::VOBJ);
  response.pushKV("mnemonic", UniValue(mnemonic));
  response.pushKV("master", UniValue(seedInfo.GetExtKey58().ToString()));
  response.pushKV("entropy", UniValue(seedInfo.GetHexEntropy()));
  return response;
}

UniValue mnemonicinfo(const JSONRPCRequest &request) {
  std::string mnemonic = "";
  if (request.params.size() > 1) {
    mnemonic = request.params[1].get_str();
  } else {
    throw std::runtime_error("missing required first argument <mnemonic>");
  }
  std::string passphrase = "";
  if (request.params.size() > 2) {
    passphrase = request.params[2].get_str();
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

UniValue mnemoniclistlanguages(const JSONRPCRequest &request) {
  UniValue response(UniValue::VOBJ);
  for (size_t ix = 0; ix < static_cast<int>(key::mnemonic::Language::COUNT);
       ++ix) {
    const auto language = static_cast<key::mnemonic::Language>(ix);
    response.pushKV(key::mnemonic::GetLanguageTag(language),
                    UniValue(key::mnemonic::GetLanguageDesc(language)));
  }
  return response;
}

UniValue mnemonic(const JSONRPCRequest &request) {
  static const std::string help =
      ""
      "mnemonic new|decode|addchecksum|dumpwords|listlanguages\n"
      "mnemonic new [password]\n"
      "    Generate a new mnemonic seed for setting a master\n"
      "    key for the hierarchical deterministic wallet.\n"
      "mnemonic info <mnemonic> [password]\n"
      "    Shows various kinds of information about a mnemonic seed:\n"
      "    \"language\": the language detected from the words,\n"
      "    \"bip39_seed\": the seed decoded and converted to hex,\n"
      "    \"bip32_root\": the private key derived from this seed,\n"
      "    \"entropy\": the entropy contained in this seed.\n"
      "mnemonic listlanguages\n"
      "    Print list of supported languages.\n"
      "\n";
  if (request.fHelp || request.params.size() == 0) {
    throw std::runtime_error(help);
  }
  std::string subcommand = request.params[0].get_str();

  if (subcommand == "new") {
    return mnemonicnew(request);
  } else if (subcommand == "info") {
    return mnemonicinfo(request);
  } else if (subcommand == "listlanguages") {
    return mnemoniclistlanguages(request);
  } else {
    throw std::runtime_error("unknown mnemonic subcommand: " + subcommand);
  }
}

UniValue importmasterkey(const JSONRPCRequest &request) {
  static const std::string help =
      "importmasterkey\n"
      "\nImport a master key from a BIP39 seed, with an optional passphrase."
      "\nArguments:\n"
      "1. \"seed\"       (string, required) a list of words to create the "
      "master key from\n"
      "2. \"passphrase\" (string, optional) an optional passphrase to "
      "protect the key\n"
      "3. \"rescan\" (bool, optional) an optional flag whether to rescan "
      "the blockchain"
      "\nExamples:\n" +
      HelpExampleCli("importmasterkey",
                     "\"next debate force grief bleak want truck prepare "
                     "theme lecture wear century rich grace someone\"") +
      HelpExampleRpc("importmasterkey",
                     "\"next debate force grief bleak want truck prepare "
                     "theme lecture wear century rich grace someone\"");
  if (request.fHelp || request.params.size() > 2 || request.params.size() < 1) {
    throw std::runtime_error(help);
  }
  CWallet *wallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(wallet, request.fHelp)) {
    throw std::runtime_error("no unlocked wallet open!");
  }
  bool shouldRescan = true;
  if (request.params.size() > 2) {
    shouldRescan = request.params[2].get_bool();
  }
  if (shouldRescan && fPruneMode) {
    throw JSONRPCError(RPC_WALLET_ERROR, "Rescan is disabled in pruned mode");
  }
  const std::string walletFileName = wallet->GetName();
  std::string mnemonic(request.params[0].get_str());
  std::string passphrase;
  if (request.params.size() > 1) {
    passphrase = request.params[1].get_str();
  }
  key::mnemonic::Seed seed(mnemonic, passphrase);
  std::string error;
  std::vector<std::string> warnings;
  {
    LOCK2(cs_main, wallet->cs_wallet);
    WalletRescanReserver reserver(wallet);
    if (shouldRescan && !reserver.reserve()) {
      throw JSONRPCError(
          RPC_WALLET_ERROR,
          "Wallet is currently rescanning. Abort existing rescan or wait.");
    }
    if (!wallet->GetWalletExtension().SetMasterKeyFromSeed(seed, error)) {
      throw std::runtime_error(error);
    }
    const int64_t rescannedTill = wallet->RescanFromTime(TIMESTAMP_MIN, reserver, /* update */ true);
    if (rescannedTill > TIMESTAMP_MIN) {
      warnings.emplace_back("could not read before " + DateTimeToString(rescannedTill));
    }
    wallet->ReacceptWalletTransactions();
  }
  UniValue response(UniValue::VOBJ);
  response.pushKV("wallet", UniValue(walletFileName));
  response.pushKV("language", UniValue(seed.GetHumandReadableLanguage()));
  response.pushKV("language_tag", UniValue(seed.GetLanguageTag()));
  response.pushKV("bip39_seed", UniValue(seed.GetHexSeed()));
  response.pushKV("bip32_root", UniValue(seed.GetExtKey58().ToString()));
  UniValue warningsValue(UniValue::VARR);
  for (const auto &warning : warnings) {
    warningsValue.push_back(UniValue(warning));
  }
  response.pushKV("warnings", warningsValue);
  response.pushKV("success", UniValue(true));
  return response;
}

/**
 * Internal, for functional tests.
 *
 * Dumps all the reserve keys for verifying the mnemonic seed generates the same
 * wallet deterministically.
 */
UniValue listreservekeys(const JSONRPCRequest &request) {
  CWallet *const wallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(wallet, request.fHelp)) {
    throw std::runtime_error("no unlocked wallet open!");
  }
  const std::map<CKeyID, int64_t> allReserveKeys = wallet->GetAllReserveKeys();
  UniValue reserveKeys(UniValue::VARR);
  for (auto &it : allReserveKeys) {
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
static const CRPCCommand commands[] = {
//  category               name                      actor (function)         argNames
//  ---------------------  ------------------------  -----------------------  ------------------------------------------
    { "mnemonic",          "mnemonic",               &mnemonic,               {"subcommand", "mnemonic", "passphrase"}},
    { "hidden",            "listreservekeys",        &listreservekeys,        {}},
    { "wallet",            "importmasterkey",        &importmasterkey,        {"seed", "passphrase", "rescan"}},
};
// clang-format on

void RegisterMnemonicRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
