// Copyright (c) 2018 The Particl Core developers
// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <extkey.h>
#include <key.h>
#include <key_io.h>
#include <rpc/protocol.h>
#include <rpc/server.h>
#include <usbdevice/usbdevice.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

#include <memory>

static std::shared_ptr<usbdevice::USBDevice> SelectDevice() {
  std::string error;
  auto result = usbdevice::SelectDevice(error);
  if (!result) {
    throw JSONRPCError(RPC_INTERNAL_ERROR, error);
  }
  return result;
}

static std::vector<uint32_t> GetFullPath(const UniValue &path, const UniValue &account_path) {
  if (!account_path.isNull() && !account_path.isStr()) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, _("Unknown \"account_path\" type."));
  }

  if (!path.isStr()) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, _("Unknown \"path\" type."));
  }

  std::string full_path = path.get_str();
  if (account_path.isNull()) {
    full_path = DEFAULT_ACCOUNT_PATH + "/" + full_path;
  } else if (account_path.isStr() && !account_path.get_str().empty()) {
    full_path = account_path.get_str() + "/" + full_path;
  }

  std::vector<uint32_t> path_result;
  std::string error;
  if (!ParseExtKeyPath(full_path, path_result, error)) {
    throw JSONRPCError(
        RPC_INVALID_PARAMETER,
        strprintf("Cannot parse path %s: %s.", full_path, error));
  }

  return path_result;
}

static UniValue listdevices(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() > 0) {
    throw std::runtime_error(
        "listdevices\n"
        "list connected hardware devices.\n"
        "\nResult\n"
        "{\n"
        "  \"vendor\"           (string) USB vendor string.\n"
        "  \"product\"          (string) USB product string.\n"
        "  \"serial_no\"        (string) Device serial number.\n"
        "  \"firmware_version\" (string, optional) Detected firmware version of the device, if available.\n"
        "}\n"
        "\nExamples\n" +
        HelpExampleCli("listdevices", "") +
        "\nAs a JSON-RPC call\n" + HelpExampleRpc("listdevices", ""));
  }

  usbdevice::DeviceList devices;
  ListAllDevices(devices);

  UniValue result(UniValue::VARR);

  for (const auto &device : devices) {
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("vendor", device->m_type.m_vendor);
    obj.pushKV("product", device->m_type.m_product);
    obj.pushKV("serial_no", device->m_serial_no);

    std::string firmware, error;
    if (device->GetFirmwareVersion(firmware, error)) {
      obj.pushKV("firmware_version", firmware);
    } else {
      obj.pushKV("error", error);
#ifndef WIN32
#ifndef MAC_OSX
      obj.pushKV("tip", "Have you set udev rules?");
#endif
#endif
    }

    result.push_back(obj);
  }

  return result;
}

static UniValue getdevicepubkey(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
    throw std::runtime_error(
        "getdevicepubkey \"path\" (\"account_path\")\n"
        "Get the public key and address at \"path\" from a hardware device.\n"
        "\nArguments:\n"
        "1. \"path\"              (string, required) The path to derive the key from.\n"
        "2. \"account_path\"      (string, optional) Account path, set to empty string to ignore (default=\"" +
        DEFAULT_ACCOUNT_PATH +
        "\").\n"
        "\nResult\n"
        "{\n"
        "  \"pubkey\"           (string) The hex-encoded derived public key at \"path\".\n"
        "  \"address\"          (string) The address of \"pubkey\".\n"
        "  \"path\"             (string) The full path of \"pubkey\".\n"
        "}\n"
        "\nExamples\n"
        "Get the first public key of external chain:\n" +
        HelpExampleCli("getdevicepubkey", "\"0/0\"") + "Get the first public key of the internal chain of a testnet account:\n" + HelpExampleCli("getdevicepubkey", "\"0/0\" \"44h/1h/0h\"") +
        "\nAs a JSON-RPC call\n" + HelpExampleRpc("getdevicepubkey", "\"0/0\""));
  }

  std::vector<uint32_t> path = GetFullPath(request.params[0], request.params[1]);

  const auto device = SelectDevice();
  std::string error;
  CPubKey pk;

  if (!device->GetPubKey(path, pk, error)) {
    throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("GetPubKey failed: %s.", error));
  }

  UniValue rv(UniValue::VOBJ);
  rv.pushKV("pubkey", HexStr(pk));
  rv.pushKV("address", EncodeDestination(pk.GetID()));
  rv.pushKV("path", FormatExtKeyPath(path));

  return rv;
}

static UniValue getdeviceextpubkey(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
    throw std::runtime_error(
        "getdeviceextpubkey \"path\" (\"account_path\")\n"
        "Get the extended public key at \"path\" from a hardware device.\n"
        "\nArguments:\n"
        "1. \"path\"              (string, required) The path to derive the key from.\n"
        "                           The full path is \"account_path\"/\"path\".\n"
        "2. \"account_path\"      (string, optional) Account path, set to empty string to ignore (default=\"" +
        DEFAULT_ACCOUNT_PATH +
        "\").\n"
        "\nResult\n"
        "{\n"
        "  \"extpubkey\"          (string) The hex-encoded derived extended public key at \"path\".\n"
        "  \"path\"               (string) The full path of \"extpubkey\".\n"
        "}\n"
        "\nExamples\n" +
        HelpExampleCli("getdeviceextpubkey", "\"0\"") +
        "\nAs a JSON-RPC call\n" + HelpExampleRpc("getdeviceextpubkey", "\"0\""));
  }

  std::vector<uint32_t> path = GetFullPath(request.params[0], request.params[1]);
  const auto device = SelectDevice();
  std::string error;
  CExtPubKey ekp;

  if (!device->GetExtPubKey(path, ekp, error)) {
    throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("GetExtPubKey failed: %s.", error));
  }

  UniValue rv(UniValue::VOBJ);
  rv.pushKV("extpubkey", ExtKeyToString(ekp));
  rv.pushKV("path", FormatExtKeyPath(path));

  return rv;
}

static const int64_t TIMESTAMP_MIN = 0;

static UniValue initaccountfromdevice(const JSONRPCRequest &request) {
  std::shared_ptr<CWallet> wallet = GetWalletForJSONRPCRequest(request);
  CWallet *pwallet = wallet.get();
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() > 1) {
    throw std::runtime_error(
        "initaccountfromdevice (\"account_path\")\n"
        "Initialise an extended key account from a hardware device.\n" +
        HelpRequiringPassphrase(pwallet) +
        "\nArguments:\n"
        "1. \"account_path\"              (string, optional) The path to "
        "derive the key from (default=\"" +
        DEFAULT_ACCOUNT_PATH +
        "\").\n"
        "\nResult\n"
        "{\n"
        "  \"extpubkey\"        (string) The derived extended public key at \"path\".\n"
        "  \"path\"             (string) The path used to derive the account.\n"
        "}\n"
        "\nExamples\n" +
        HelpExampleCli("initaccountfromdevice", "\"m/44'/600'/0'\"") +
        "\nAs a JSON-RPC call\n" + HelpExampleRpc("initaccountfromdevice", "\"m/44'/600'/0'\""));
  }

  RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VSTR, UniValue::VBOOL, UniValue::VNUM}, true);

  // Make sure the results are valid at least up to the most recent block
  // the user could have gotten from another RPC command prior to now
  pwallet->BlockUntilSyncedToCurrentChain();

  EnsureWalletIsUnlocked(pwallet);

  std::string path_string = DEFAULT_ACCOUNT_PATH;
  if (request.params[0].isStr() && request.params[0].get_str().size()) {
    path_string = request.params[0].get_str();
  }

  WalletRescanReserver reserver(pwallet);
  if (!reserver.reserve()) {
    throw JSONRPCError(RPC_WALLET_ERROR, "Wallet is currently rescanning. Abort existing rescan or wait.");
  }

  const auto device = SelectDevice();

  std::vector<uint32_t> path;
  std::string error;

  CPubKey master_key;
  if (!device->GetPubKey(path, master_key, error)) {
    throw JSONRPCError(
        RPC_INTERNAL_ERROR,
        strprintf("Cannot retrieve master key: %s.", error));
  }

  if (!ParseExtKeyPath(path_string, path, error)) {
    throw JSONRPCError(
        RPC_INVALID_PARAMETER,
        strprintf("Cannt parse path %s : %s.", path_string, error));
  }

  CExtPubKey acctKey;
  if (!device->GetExtPubKey(path, acctKey, error)) {
    throw JSONRPCError(
        RPC_INTERNAL_ERROR,
        strprintf("Cannot retrieve account key: %s.", error));
  }

  // Serialize it back to get a uniform representation
  path_string = FormatExtKeyPath(path);

  {
    LOCK2(cs_main, pwallet->cs_wallet);
    auto locked_chain = pwallet->chain().lock();
    WalletBatch wdb(pwallet->GetDBHandle(), "r+");

    int64_t creation_time = GetTime();
    CKeyMetadata metadata(creation_time);
    metadata.hdKeypath = path_string;

    pwallet->SetHDMasterKey(master_key, acctKey, metadata, true);
    pwallet->NewKeyPool();

    pwallet->RescanFromTime(TIMESTAMP_MIN, reserver, true /* update */);
    pwallet->MarkDirty();
    pwallet->ReacceptWalletTransactions(*locked_chain);
  }  // pwallet->cs_wallet

  UniValue result(UniValue::VOBJ);
  result.pushKV("extpubkey", ExtKeyToString(acctKey));
  result.pushKV("path", path_string);

  return result;
}

// clang-format off
static const CRPCCommand commands[] = {
//  category              name                            actor (function)            argNames
//  --------------------- ------------------------        -----------------------     ----------
    {"usbdevice",         "listdevices",                  &listdevices,               {}},
    {"usbdevice",         "getdevicepubkey",              &getdevicepubkey,           {"path", "accountpath"}},
    {"usbdevice",         "getdeviceextpubkey",           &getdeviceextpubkey,        {"path", "accountpath"}},
    {"usbdevice",         "initaccountfromdevice",        &initaccountfromdevice,     {"path"}},
};
// clang-format on

void RegisterUSBDeviceRPC(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
