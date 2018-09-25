// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <wallet/rpcwalletext.h>

#include <consensus/validation.h>
#include <core_io.h>
#include <net.h>
#include <policy/policy.h>
#include <rpc/mining.h>
#include <rpc/safemode.h>
#include <rpc/server.h>
#include <univalue.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

static CRecipient ParseOutputArgument(const UniValue &output) {
  if (!output.isObject()) {
    throw JSONRPCError(RPC_TYPE_ERROR, "Not an object");
  }
  const UniValue &obj = output.get_obj();

  std::string address;
  CAmount amount;

  if (obj.exists("address")) {
    address = obj["address"].get_str();
  } else {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Must provide an address.");
  }

  CScript scriptPubKey;
  if (obj.exists("script")) {
    if (address != "script") {
      JSONRPCError(
          RPC_INVALID_PARAMETER,
          "address parameter must be 'script' to set script explicitly.");
    }

    std::string sScript = obj["script"].get_str();
    std::vector<uint8_t> scriptData = ParseHex(sScript);
    scriptPubKey = CScript(scriptData.begin(), scriptData.end());
  } else {
    CTxDestination dest = DecodeDestination(address);

    if (!IsValidDestination(dest)) {
      throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid UnitE address");
    }
    scriptPubKey = GetScriptForDestination(dest);
  }

  if (obj.exists("amount")) {
    amount = AmountFromValue(obj["amount"]);
  } else {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Must provide an amount.");
  }

  if (amount <= 0) {
    throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
  }

  bool subtractFeeFromAmount = false;
  if (obj.exists("subfee")) {
    subtractFeeFromAmount = obj["subfee"].get_bool();
  }

  return {scriptPubKey, amount, subtractFeeFromAmount};
}

static CCoinControl ParseCoinControlArgument(const UniValue &uvCoinControl) {
  CCoinControl coinControl;
  if (uvCoinControl.exists("changeaddress")) {
    CTxDestination dest =
        DecodeDestination(uvCoinControl["changeaddress"].get_str());

    if (!IsValidDestination(dest)) {
      throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                         "changeAddress must be a valid unite address");
    }

    coinControl.destChange = dest;
  }

  const UniValue &uvInputs = uvCoinControl["inputs"];
  if (uvInputs.isArray()) {
    for (size_t i = 0; i < uvInputs.size(); ++i) {
      const UniValue &uvInput = uvInputs[i];
      RPCTypeCheckObj(uvInput, {
                                   {"tx", UniValueType(UniValue::VSTR)},
                                   {"n", UniValueType(UniValue::VNUM)},
                               });

      COutPoint op(uint256S(uvInput["tx"].get_str()), uvInput["n"].get_int());
      coinControl.Select(op);
    }
  }

  if (uvCoinControl.exists("fee_rate") &&
      uvCoinControl.exists("estimate_mode")) {
    throw JSONRPCError(RPC_INVALID_PARAMETER,
                       "Cannot specify both estimate_mode and fee_rate");
  }
  if (uvCoinControl.exists("fee_rate") && uvCoinControl.exists("conf_target")) {
    throw JSONRPCError(RPC_INVALID_PARAMETER,
                       "Cannot specify both conf_target and fee_rate");
  }

  if (uvCoinControl.exists("replaceable")) {
    if (!uvCoinControl["replaceable"].isBool()) {
      throw JSONRPCError(RPC_INVALID_PARAMETER,
                         "Replaceable parameter must be boolean.");
    }
    coinControl.signalRbf = uvCoinControl["replaceable"].get_bool();
  }

  if (uvCoinControl.exists("conf_target")) {
    if (!uvCoinControl["conf_target"].isNum()) {
      throw JSONRPCError(RPC_INVALID_PARAMETER,
                         "conf_target parameter must be numeric.");
    }
    coinControl.m_confirm_target =
        ParseConfirmTarget(uvCoinControl["conf_target"]);
  }

  if (uvCoinControl.exists("estimate_mode")) {
    if (!uvCoinControl["estimate_mode"].isStr()) {
      throw JSONRPCError(RPC_INVALID_PARAMETER,
                         "estimate_mode parameter must be a string.");
    }
    if (!FeeModeFromString(uvCoinControl["estimate_mode"].get_str(),
                           coinControl.m_fee_mode)) {
      throw JSONRPCError(RPC_INVALID_PARAMETER,
                         "Invalid estimate_mode parameter");
    }
  }

  if (uvCoinControl.exists("fee_rate")) {
    coinControl.m_feerate =
        CFeeRate(AmountFromValue(uvCoinControl["fee_rate"]));
    coinControl.fOverrideFeeRate = true;
  }

  return coinControl;
}

UniValue sendtypeto(const JSONRPCRequest &request) {
  CWallet *pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() < 3 || request.params.size() > 9) {
    throw std::runtime_error(
        "sendtypeto \"typein\" \"typeout\" [{address: , amount: , narr: , "
        "subfee:},...] (\"comment\" \"comment-to\" test_fee coin_control)\n"
        "\nSend UnitE to multiple outputs.\n" +
        HelpRequiringPassphrase(pwallet) +
        "\nArguments:\n"
        "1. \"typein\"          (string, required) \n"
        "2. \"typeout\"         (string, required) \n"
        "3. outputs           (json, required) Array of output objects\n"
        "    3.1 \"address\"    (string, required) The UnitE address to send "
        "to.\n"
        "    3.2 \"amount\"     (numeric or string, required) The amount in " +
        CURRENCY_UNIT +
        " to send. eg 0.1\n"
        "    3.x \"subfee\"     (boolean, optional, default=false) The fee "
        "will be deducted from the amount being sent.\n"
        "    3.x \"script\"     (string, optional) Hex encoded script, will "
        "override the address.\n"
        "4. \"comment\"         (string, optional) A comment used to store "
        "what the transaction is for. \n"
        "                       This is not part of the transaction, just kept "
        "in your wallet.\n"
        "5. \"comment_to\"      (string, optional) A comment to store the name "
        "of the person or organization \n"
        "                       to which you're sending the transaction. This "
        "is not part of the \n"
        "                       transaction, just kept in your wallet.\n"
        "6. test_fee          (bool, optional, default=false) Only return the "
        "fee it would cost to send, txn is discarded.\n"
        "7. coin_control      (json, optional) Coincontrol object.\n"
        "    7.1 \"changeaddress\"  (string, optional) The Address for "
        "receiving change\n"
        "    7.2 \"inputs\"         (json, optional) \n"
        "           [{\"tx\":, \"n\":},...],\n"
        "    7.3 \"replaceable\"    (boolean, optional)  Allow this "
        "transaction to be replaced by a transaction\n"
        "                           with higher fees via BIP 125\n"
        "    7.4 \"conf_target\"    (numeric, optional) Confirmation target "
        "(in blocks)\n"
        "    7.5 \"estimate_mode\"  (string, optional) The fee estimate mode, "
        "must be one of:\n"
        "            \"UNSET\"\n"
        "            \"ECONOMICAL\"\n"
        "            \"CONSERVATIVE\"\n"
        "    7.6 \"fee_rate\"        (numeric, optional, default not set: "
        "makes wallet determine the fee) Set a specific \n"
        "                           feerate (" +
        CURRENCY_UNIT +
        " per KB)\n"
        "\nResult:\n"
        "\"txid\"              (string) The transaction id.\n"
        "\nExamples:\n" +
        HelpExampleCli(
            "sendtypeto",
            "unit unit "
            "\"[{\\\"address\\\":\\\"2NDoNG8nR57LDs9m2VKV4wzYVR9YBJ2L5Nd\\\","
            "\\\"amount\\\":0.1}]\""));
  }

  ObserveSafeMode();

  // Make sure the results are valid at least up to the most recent block
  // the user could have gotten from another RPC command prior to now
  pwallet->BlockUntilSyncedToCurrentChain();

  if (pwallet->GetBroadcastTransactions() && !g_connman) {
    throw JSONRPCError(RPC_CLIENT_P2P_DISABLED,
                       "Error: Peer-to-peer functionality missing or disabled");
  }

  // UNIT-E: typeIn and typeOut will be used in the future
  // std::string typeIn = request.params[0].get_str();
  // std::string typeOut = request.params[1].get_str();

  CAmount totalAmount = 0;
  std::vector<CRecipient> vecSend;

  if (!request.params[2].isArray()) {
    throw JSONRPCError(RPC_TYPE_ERROR, "Not an array");
  }
  const UniValue &outputs = request.params[2].get_array();
  for (size_t k = 0; k < outputs.size(); ++k) {
    CRecipient recipient = ParseOutputArgument(outputs[k]);
    vecSend.push_back(recipient);
    totalAmount += recipient.nAmount;
  }

  EnsureWalletIsUnlocked(pwallet);

  if (totalAmount > pwallet->GetBalance()) {
    throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");
  }

  // Wallet comments
  CWalletTx wtx;

  if (request.params.size() > 3 && !request.params[3].isNull()) {
    std::string s = request.params[3].get_str();
    if (!s.empty()) {
      wtx.mapValue["comment"] = s;
    }
  }
  if (request.params.size() > 4 && !request.params[4].isNull()) {
    std::string s = request.params[4].get_str();
    if (!s.empty()) {
      wtx.mapValue["to"] = s;
    }
  }

  bool checkFeeOnly = false;
  if (request.params.size() > 5) {
    checkFeeOnly = request.params[5].get_bool();
  }

  bool showHex = false;
  CCoinControl coinControl;

  if (request.params.size() > 6 && request.params[6].isObject()) {
    const UniValue &uvCoinControl = request.params[6].get_obj();
    coinControl = ParseCoinControlArgument(uvCoinControl);

    if (uvCoinControl["debug"].isBool() && uvCoinControl["debug"].get_bool()) {
      showHex = true;
    }
  }

  CAmount feeRet = 0;
  CReserveKey keyChange(pwallet);
  int changePosRet = -1;
  std::string failReason;
  bool created = pwallet->CreateTransaction(
      vecSend, wtx, keyChange, feeRet, changePosRet, failReason, coinControl);
  if (!created) {
    throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, failReason);
  }

  if (checkFeeOnly) {
    UniValue result(UniValue::VOBJ);
    result.pushKV("fee", ValueFromAmount(feeRet));
    result.pushKV("bytes", (int)GetVirtualTransactionSize(*(wtx.tx)));

    if (showHex) {
      std::string strHex = EncodeHexTx(*(wtx.tx), RPCSerializationFlags());
      result.pushKV("hex", strHex);
    }

    return result;
  }

  CValidationState state;
  if (!pwallet->CommitTransaction(wtx, keyChange, g_connman.get(), state)) {
    throw JSONRPCError(
        RPC_WALLET_ERROR,
        strprintf("Transaction commit failed: %s", FormatStateMessage(state)));
  }

  return wtx.GetHash().GetHex();
}

// clang-format off
static const CRPCCommand commands[] = {
//  category               name                      actor (function)         argNames
//  ---------------------  ------------------------  -----------------------  ------------------------------------------
  {"wallet",             "sendtypeto",             &sendtypeto,             {"typein","typeout","outputs","comment","comment_to","test_fee","coincontrol"}},
};
// clang-format on

void RegisterWalletextRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
