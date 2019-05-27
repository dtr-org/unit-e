// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <wallet/rpcwalletext.h>

#include <consensus/validation.h>
#include <core_io.h>
#include <injector.h>
#include <key_io.h>
#include <net.h>
#include <policy/policy.h>
#include <rpc/mining.h>
#include <rpc/server.h>
#include <univalue.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

static CRecipient ParseOutputArgument(const UniValue &output, bool allow_script = true) {
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
  if (obj.exists("script") && !allow_script) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid key: 'script'");
  }
  if (obj.exists("script")) {
    if (address != "script") {
      throw JSONRPCError(
          RPC_INVALID_PARAMETER,
          "address parameter must be 'script' to set script explicitly.");
    }

    std::string sScript = obj["script"].get_str();
    std::vector<uint8_t> scriptData = ParseHex(sScript);
    scriptPubKey = CScript(scriptData.begin(), scriptData.end());
  } else {
    CTxDestination dest = DecodeDestination(address);

    if (!IsValidDestination(dest)) {
      throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Unit-e address");
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
                         "changeAddress must be a valid Unit-e address");
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
    coinControl.m_signal_bip125_rbf = uvCoinControl["replaceable"].get_bool();
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

  if (uvCoinControl.exists("ignore_remote_staked")) {
    coinControl.m_ignore_remote_staked =
        uvCoinControl["ignore_remote_staked"].get_bool();
  }

  return coinControl;
}

UniValue sendtypeto(const JSONRPCRequest &request) {
  std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
  CWallet * pwallet = wallet.get();
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() < 3 || request.params.size() > 9) {
    throw std::runtime_error(
        "sendtypeto \"typein\" \"typeout\" [{address: , amount: , narr: , "
        "subfee:},...] (\"comment\" \"comment-to\" test_fee coin_control)\n"
        "\nSend Unit-e to multiple outputs.\n" +
        HelpRequiringPassphrase(pwallet) +
        "\nArguments:\n"
        "1. \"typein\"          (string, required) \n"
        "2. \"typeout\"         (string, required) \n"
        "3. outputs           (json, required)\n"
        "  [                  (Array of output objects)\n"
        "    {\n"
        "      \"address\": \"<address>\", (string, required) The Unit-e address to send "
        "to.\n"
        "      \"amount\": x.xxx,        (numeric or string, required) The amount in " +
        CURRENCY_UNIT +
        " to send. eg 0.1\n"
        "      \"subfee\": b,            (boolean, optional, default=false) The fee "
        "will be deducted from the amount being sent.\n"
        "      \"script\": \"<script>\"    (string, optional) Hex encoded script, will "
        "override the address.\n"
        "    }\n"
        "    ,...\n"
        "  ]\n"
        "4. \"comment\"         (string, optional) A comment used to store "
        "what the transaction is for. \n"
        "                     This is not part of the transaction, just kept "
        "in your wallet.\n"
        "5. \"comment_to\"      (string, optional) A comment to store the name "
        "of the person or organization \n"
        "                     to which you're sending the transaction. This "
        "is not part of the \n"
        "                     transaction, just kept in your wallet.\n"
        "6. test_fee          (bool, optional, default=false) Only return the "
        "fee it would cost to send, txn is discarded.\n"
        "7. coin_control      (json, optional) Coincontrol object.\n"
        "  {\n"
        "    \"changeaddress\": \"<address>\", (string, optional) The Address for "
        "receiving change\n"
        "    \"inputs\":                     (json, optional)\n"
        "           [{\"tx\":, \"n\":},...],\n"
        "    \"replaceable\": b,             (boolean, optional)  Allow this "
        "transaction to be replaced by a transaction\n"
        "                                  with higher fees via BIP 125\n"
        "    \"conf_target\": n,             (numeric, optional) Confirmation target "
        "(in blocks)\n"
        "    \"estimate_mode\": \"xxx\",       (string, optional) The fee estimate mode, "
        "must be one of:\n"
        "            \"UNSET\"\n"
        "            \"ECONOMICAL\"\n"
        "            \"CONSERVATIVE\"\n"
        "    \"fee_rate\": n,                (numeric, optional, default not set: "
        "makes wallet determine the fee) Set a specific\n"
        "                                  feerate (" +
        CURRENCY_UNIT +
        " per KB)\n"
        "    \"ignore_remote_staked\": b     (boolean, optional, default=false) "
        "Exclude coins that are currently staked on other nodes.\n"
        "  }\n"
        "\nResult:\n"
        "\"txid\"              (string) The transaction id.\n"
        "\nExamples:\n" +
        HelpExampleCli(
            "sendtypeto",
            "unit unit "
            "\"[{\\\"address\\\":\\\"2NDoNG8nR57LDs9m2VKV4wzYVR9YBJ2L5Nd\\\","
            "\\\"amount\\\":0.1}]\""));
  }

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
  CTransactionRef wtx;
  mapValue_t map_value;

  if (request.params.size() > 3 && !request.params[3].isNull()) {
    std::string s = request.params[3].get_str();
    if (!s.empty()) {
      map_value["comment"] = s;
    }
  }
  if (request.params.size() > 4 && !request.params[4].isNull()) {
    std::string s = request.params[4].get_str();
    if (!s.empty()) {
      map_value["to"] = s;
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
  auto locked_chain = pwallet->chain().lock();
  bool created = pwallet->CreateTransaction(
      *locked_chain, vecSend, wtx, keyChange, feeRet, changePosRet, failReason, coinControl);
  if (!created) {
    throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, failReason);
  }

  if (checkFeeOnly) {
    UniValue result(UniValue::VOBJ);
    result.pushKV("fee", ValueFromAmount(feeRet));
    result.pushKV("bytes", GetVirtualTransactionSize(*wtx));

    if (showHex) {
      std::string strHex = EncodeHexTx(*wtx, RPCSerializationFlags());
      result.pushKV("hex", strHex);
    }

    return result;
  }

  CValidationState state;
  if (!pwallet->CommitTransaction(wtx, map_value, {}, keyChange, g_connman.get(), state)) {
    throw JSONRPCError(
        RPC_WALLET_ERROR,
        strprintf("Transaction commit failed: %s", FormatStateMessage(state)));
  }

  return wtx->GetHash().GetHex();
}

constexpr const char* STAKEAT_HELP = "stakeat recipient test_fee coin_control\n"
"\nDelegate staking to the provided recipient. The funds will still be spendable\n"
"by the current node.\n%s\n"
"Arguments:\n"
"1. recipient         (json, required)\n"
"  {\n"
"    \"address\": \"address\", (string, required) The Unit-e address to send to.\n"
"    \"amount\": x.xxx,      (numeric or string, required) The amount in %s "
"to send, e.g. 0.1\n"
"    \"subfee\": b           (boolean, optional, default=false) Deduct the fee "
"from the amount being sent.\n"
"  }\n"
"2. test_fee          (bool, optional, default=false) Only return the fee it "
"would cost to send, txn is discarded.\n"
"3. coin_control      (json, optional) Coincontrol object.\n"
"  {\n"
"    \"changeaddress\": \"address\", (string, optional) The Address for "
"receiving change\n"
"    \"inputs\":                   (json, optional)\n"
"       [{\"tx\":, \"n\":},...],\n"
"    \"replaceable\": b,           (boolean, optional)  Allow this "
"transaction to be replaced by a transaction\n"
"                                with higher fees via BIP 125\n"
"    \"conf_target\": n,           (numeric, optional) Confirmation target "
"(in blocks)\n"
"    \"estimate_mode\": \"xxx\",     (string, optional) The fee estimate mode, "
"must be one of:\n"
"        \"UNSET\"\n"
"        \"ECONOMICAL\"\n"
"        \"CONSERVATIVE\"\n"
"    \"fee_rate\": n,              (numeric, optional, default not set: "
"makes wallet determine the fee) Set a specific\n"
"                                feerate (%s per KB)\n"
"    \"ignore_remote_staked\": b   (boolean, optional, default=false) "
"Exclude coins that are currently staked on other nodes.\n"
"  }\n"
"\nResult:\n"
"\"txid\"              (string) The transaction id.\n"
"\nExamples:\n%s";

constexpr const char* STAKEAT_CLI_PARAMS = "\"{\\\"address\\\":\\\"2NDoNG8nR57LDs9m2VKV4wzYVR9YBJ2L5Nd\\\","
  "\\\"amount\\\":0.1}\"";

static UniValue stakeat(const JSONRPCRequest &request) {
  std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
  CWallet * const pwallet = wallet.get();
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() < 1 || request.params.size() > 3) {
    std::string help_text = strprintf(STAKEAT_HELP,
                                      HelpRequiringPassphrase(pwallet),
                                      CURRENCY_UNIT, CURRENCY_UNIT,
                                      HelpExampleCli("stakeat", STAKEAT_CLI_PARAMS));
    throw std::runtime_error(help_text);
  }

  // Make sure the results are valid at least up to the most recent block
  // the user could have gotten from another RPC command prior to now
  pwallet->BlockUntilSyncedToCurrentChain();

  if (pwallet->GetBroadcastTransactions() && !g_connman) {
    throw JSONRPCError(RPC_CLIENT_P2P_DISABLED,
                       "Error: Peer-to-peer functionality missing or disabled");
  }

  CRecipient recipient = ParseOutputArgument(request.params[0].get_obj(), false);
  CAmount total_amount = recipient.nAmount;

  EnsureWalletIsUnlocked(pwallet);

  if (total_amount > pwallet->GetBalance()) {
    throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");
  }

  bool test_fee = false;
  if (request.params.size() > 1) {
    test_fee = request.params[1].get_bool();
  }

  CCoinControl coin_control;
  if (request.params.size() > 2 && request.params[2].isObject()) {
    coin_control = ParseCoinControlArgument(request.params[2].get_obj());
  }

  std::string error;
  CAmount tx_fee(0);
  CTransactionRef wtx;
  CReserveKey key_change(pwallet);

  auto &wallet_ext = pwallet->GetWalletExtension();
  bool created = wallet_ext.CreateRemoteStakingTransaction(
      recipient, &wtx, &key_change, &tx_fee, &error, coin_control);
  if (!created) {
    throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, error);
  }

  if (test_fee) {
    UniValue result(UniValue::VOBJ);
    result.pushKV("fee", ValueFromAmount(tx_fee));
    result.pushKV("bytes", GetVirtualTransactionSize(*wtx));

    return result;
  }

  CValidationState state;
  if (!pwallet->CommitTransaction(wtx, {}, {}, key_change, g_connman.get(), state)) {
    throw JSONRPCError(
        RPC_WALLET_ERROR,
        strprintf("Transaction commit failed: %s", FormatStateMessage(state)));
  }

  return wtx->GetHash().GetHex();
}

static bool OutputToJSON(UniValue &output, const COutputEntry &o,
                         CWallet *const pwallet, const CWalletTx &wtx,
                         const isminefilter &watchonly) {
  std::string key = strprintf("n%d", o.vout);
  auto mvi = wtx.mapValue.find(key);
  if (mvi != wtx.mapValue.end()) {
    output.pushKV("narration", mvi->second);
  }
  if (IsValidDestination(o.destination)) {
    output.pushKV("address", EncodeDestination(o.destination));
  }

  if (::IsMine(*pwallet, o.destination) & ISMINE_WATCH_ONLY) {
    if (watchonly & ISMINE_WATCH_ONLY) {
      output.pushKV("involvesWatchonly", true);
    } else {
      return false;
    }
  }

  if (pwallet->mapAddressBook.count(o.destination)) {
    output.pushKV("label", pwallet->mapAddressBook[o.destination].name);
  }
  output.pushKV("vout", o.vout);
  return true;
}

static bool OutputsContain(const UniValue &outputs, const std::string &search) {
  for (size_t i = 0; i < outputs.size(); ++i) {
    if (!outputs[i]["address"].isNull() &&
        outputs[i]["address"].get_str().find(search) != std::string::npos) {
      return true;
    }

    // character DOT '.' is not searched for: search "123" will find 1.23
    // and 12.3
    if (!outputs[i]["amount"].isNull() &&
        outputs[i]["amount"].getValStr().find(search) != std::string::npos) {
      return true;
    }
  }
  return false;
}

static bool TxWithOutputsToJSON(interfaces::Chain::Lock &locked_chain, const CWalletTx &wtx,
                                CWallet *const pwallet, const isminefilter &watchonly,
                                const std::string &search, UniValue &result) {
  UniValue entry(UniValue::VOBJ);

  // GetAmounts variables
  std::list<COutputEntry> list_received;
  std::list<COutputEntry> list_sent;
  CAmount fee;
  CAmount amount = 0;
  std::string sent_account;

  wtx.GetAmounts(list_received, list_sent, fee, ISMINE_ALL);

  if (wtx.IsFromMe(ISMINE_WATCH_ONLY) && !(watchonly & ISMINE_WATCH_ONLY)) {
    return false;
  }

  std::vector<std::string> addresses;
  std::vector<std::string> amounts;

  UniValue outputs(UniValue::VARR);
  // common to every type of transaction
  if (!sent_account.empty()) {
    entry.pushKV("account", sent_account);
  }
  WalletTxToJSON(pwallet->chain(), locked_chain, wtx, entry);

  if (!list_sent.empty()) {
    entry.pushKV("abandoned", wtx.isAbandoned());
  }

  std::set<int> receive_outputs;
  for (const auto &r : list_received) {
    receive_outputs.insert(r.vout);
  }

  // sent
  if (!list_sent.empty()) {
    entry.pushKV("fee", ValueFromAmount(-fee));
    for (const auto &s : list_sent) {
      UniValue output(UniValue::VOBJ);
      if (!OutputToJSON(output, s, pwallet, wtx, watchonly)) {
        return false;
      }
      amount -= s.amount;
      if (receive_outputs.count(s.vout) == 0) {
        output.pushKV("amount", ValueFromAmount(-s.amount));
        outputs.push_back(output);
      }
    }
  }

  // received
  for (const auto &r : list_received) {
    UniValue output(UniValue::VOBJ);
    if (!OutputToJSON(output, r, pwallet, wtx, watchonly)) {
      return false;
    }

    output.pushKV("amount", ValueFromAmount(r.amount));
    amount += r.amount;

    outputs.push_back(output);
  }

  if (wtx.IsCoinBase()) {
    if (!wtx.IsInMainChain(locked_chain)) {
      entry.pushKV("category", "orphan");
    } else if (wtx.GetBlocksToRewardMaturity(locked_chain) > 0) {
      entry.pushKV("category", "immature");
    } else {
      entry.pushKV("category", "coinbase");
    }
  } else if (!fee) {
    entry.pushKV("category", "receive");
  } else if (amount == 0) {
    if (list_sent.empty()) {
      entry.pushKV("fee", ValueFromAmount(-fee));
    }
    entry.pushKV("category", "internal_transfer");
  } else {
    entry.pushKV("category", "send");
  }

  entry.pushKV("outputs", outputs);
  entry.pushKV("amount", ValueFromAmount(amount));

  if (search.empty() || OutputsContain(outputs, search)) {
    result = std::move(entry);
    return true;
  }

  return false;
}

static std::string GetAddress(const UniValue &transaction) {
  if (!transaction["address"].isNull()) {
    return transaction["address"].get_str();
  }
  if (!transaction["outputs"][0]["address"].isNull()) {
    return transaction["outputs"][0]["address"].get_str();
  }
  return "";
}

UniValue filtertransactions(const JSONRPCRequest &request) {
  std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
  CWallet * const pwallet = wallet.get();
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() > 1) {
    throw std::runtime_error(
        "filtertransactions ( options )\n"
        "\nList transactions.\n"
        "\nArguments:\n"
        "1. options (json, optional) : A configuration object for the query\n"
        "\n"
        "    All keys are optional. Default values are:\n"
        "    {\n"
        "        \"count\":             10,\n"
        "        \"skip\":              0,\n"
        "        \"include_watchonly\": false,\n"
        "        \"search\":            ''\n"
        "        \"category\":          'all',\n"
        "        \"sort\":              'time'\n"
        "        \"from\":              '0'\n"
        "        \"to\":                '9999'\n"
        "        \"collate\":           false\n"
        "    }\n"
        "\n"
        "    Expected values are as follows:\n"
        "        count:             number of transactions to be displayed\n"
        "                           (integer >= 0, use 0 for unlimited)\n"
        "        skip:              number of transactions to skip\n"
        "                           (integer >= 0)\n"
        "        include_watchonly: whether to include watchOnly transactions\n"
        "                           (bool string)\n"
        "        search:            a query to search addresses and amounts\n"
        "                           character DOT '.' is not searched for:\n"
        "                           search \"123\" will find 1.23 and 12.3\n"
        "                           (query string)\n"
        "        category:          select only one category of transactions to"
        " return\n"
        "                           (string from list)\n"
        "                           all, send, orphan, immature, coinbase, \n"
        "                           receive, orphaned_stake, stake,"
        " internal_transfer\n"
        "        sort:              sort transactions by criteria\n"
        "                           (string from list)\n"
        "                           time          most recent first\n"
        "                           address       alphabetical\n"
        "                           category      alphabetical\n"
        "                           amount        biggest first\n"
        "                           confirmations most confirmations first\n"
        "                           txid          alphabetical\n"
        "        from:              unix timestamp or string "
        "\"yyyy-mm-ddThh:mm:ss\"\n"
        "        to:                unix timestamp or string "
        "\"yyyy-mm-ddThh:mm:ss\"\n"
        "        collate:           display number of records and sum of amount"
        " fields\n"
        "\nExamples:\n"
        "    List only when category is 'send'\n"
        "        " +
        HelpExampleCli("filtertransactions", R"("{\"category\":\"send\"}")") +
        "\n"
        "    Multiple arguments\n"
        "        " +
        HelpExampleCli("filtertransactions",
                       R"("{\"sort\":\"amount\", \"category\":\"receive\"}")") +
        "\n"
        "    As a JSON-RPC call\n"
        "        " +
        HelpExampleRpc("filtertransactions", R"({\"category\":\"send\"})") +
        "\n");
  }

  // Make sure the results are valid at least up to the most recent block
  // the user could have gotten from another RPC command prior to now
  pwallet->BlockUntilSyncedToCurrentChain();

  int count = 10;
  int skip = 0;
  isminefilter watchonly = ISMINE_SPENDABLE;
  std::string search;
  std::string category = "all";
  std::string sort = "time";

  int64_t timeFrom = 0;
  int64_t timeTo = 253370764800;  // 01 Jan 9999
  bool collate = false;

  if (!request.params[0].isNull()) {
    const UniValue &options = request.params[0].get_obj();
    RPCTypeCheckObj(options,
                    {
                        {"count", UniValueType(UniValue::VNUM)},
                        {"skip", UniValueType(UniValue::VNUM)},
                        {"include_watchonly", UniValueType(UniValue::VBOOL)},
                        {"search", UniValueType(UniValue::VSTR)},
                        {"category", UniValueType(UniValue::VSTR)},
                        {"sort", UniValueType(UniValue::VSTR)},
                        {"collate", UniValueType(UniValue::VBOOL)},
                    },
                    true,  // allow null
                    false  // strict
    );
    if (options.exists("count")) {
      count = options["count"].get_int();
      if (count < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           strprintf("Invalid count: %i.", count));
      }
    }
    if (options.exists("skip")) {
      skip = options["skip"].get_int();
      if (skip < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           strprintf("Invalid skip number: %i.", skip));
      }
    }
    if (options.exists("include_watchonly")) {
      if (options["include_watchonly"].get_bool()) {
        watchonly = watchonly | ISMINE_WATCH_ONLY;
      }
    }
    if (options.exists("search")) {
      search = options["search"].get_str();
    }
    if (options.exists("category")) {
      category = options["category"].get_str();
      std::vector<std::string> categories = {
          "all",     "send",           "orphan", "immature",         "coinbase",
          "receive", "orphaned_stake", "stake",  "internal_transfer"};
      auto it = std::find(categories.begin(), categories.end(), category);
      if (it == categories.end()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           strprintf("Invalid category: %s.", category));
      }
    }
    if (options.exists("sort")) {
      sort = options["sort"].get_str();
      std::vector<std::string> sorts = {"time",   "address",       "category",
                                        "amount", "confirmations", "txid"};
      auto it = std::find(sorts.begin(), sorts.end(), sort);
      if (it == sorts.end()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           strprintf("Invalid sort: %s.", sort));
      }
    }
    if (options["from"].isStr()) {
      timeFrom = StrToEpoch(options["from"].get_str());
    } else if (options["from"].isNum()) {
      timeFrom = options["from"].get_int64();
    }
    if (options["to"].isStr()) {
      timeTo = StrToEpoch(options["to"].get_str(), true);
    } else if (options["to"].isNum()) {
      timeTo = options["to"].get_int64();
    }
    if (options["collate"].isBool()) {
      collate = options["collate"].get_bool();
    }
  }

  UniValue transactions(UniValue::VARR);

  finalization::StateRepository *fin_repo = GetComponent<finalization::StateRepository>();

  {
    LOCK2(cs_main, pwallet->cs_wallet);
    LOCK(fin_repo->GetLock());

    auto locked_chain = pwallet->chain().lock();

    // transaction processing
    const CWallet::TxItems &txOrdered = pwallet->wtxOrdered;
    for (auto tit = txOrdered.rbegin(); tit != txOrdered.rend(); ++tit) {
      CWalletTx *const pwtx = tit->second;
      int64_t txTime = pwtx->GetTxTime();
      if (txTime < timeFrom) {
        break;
      }

      UniValue entry;
      bool match_filter = false;
      if (txTime <= timeTo) {
        match_filter = TxWithOutputsToJSON(*locked_chain, *pwtx, pwallet, watchonly, search, entry);
      }
      if (!match_filter) {
        continue;
      }

      // Get the transaction finalization state
      const CBlockIndex *block_index = nullptr;
      bool finalized = false;
      if (pwtx->GetDepthInMainChain(*locked_chain) > 0) {
        const finalization::FinalizationState *tip_fin_state = fin_repo->GetTipState();
        assert(tip_fin_state != nullptr);
        finalized = tip_fin_state->GetLastFinalizedEpoch() >= tip_fin_state->GetEpoch(*block_index);
      }
      entry.pushKV("finalized", UniValue(finalized));

      transactions.push_back(entry);
    }
  }

  // sort
  std::vector<UniValue> values = transactions.getValues();
  std::sort(values.begin(), values.end(),
            [sort](const UniValue &a, const UniValue &b) -> bool {
              if (sort == "category" || sort == "txid") {
                return a[sort].get_str() < b[sort].get_str();
              }
              if (sort == "time" || sort == "confirmations") {
                return a[sort].get_real() > b[sort].get_real();
              }
              if (sort == "address") {
                std::string a_address = GetAddress(a);
                std::string b_address = GetAddress(b);
                return a_address < b_address;
              }
              if (sort == "amount") {
                double a_amount = a["category"].get_str() == "send"
                                      ? -(a["amount"].get_real())
                                      : a["amount"].get_real();
                double b_amount = b["category"].get_str() == "send"
                                      ? -(b["amount"].get_real())
                                      : b["amount"].get_real();
                return a_amount > b_amount;
              }
              return false;
            });

  // filter, skip, count and sum
  CAmount totalAmount = 0, totalReward = 0;
  UniValue result(UniValue::VARR);
  if (count == 0) {
    count = static_cast<int>(values.size());
  }

  for (unsigned int i = 0; i < values.size() && count != 0; ++i) {
    if (category != "all" && values[i]["category"].get_str() != category) {
      continue;
    }
    // if we've skipped enough valid values
    if (skip-- <= 0) {
      result.push_back(values[i]);
      --count;

      if (collate) {
        if (!values[i]["amount"].isNull()) {
          totalAmount += AmountFromValue(values[i]["amount"]);
        }
        if (!values[i]["reward"].isNull()) {
          totalReward += AmountFromValue(values[i]["reward"]);
        }
      }
    }
  }

  if (collate) {
    UniValue retObj(UniValue::VOBJ);
    UniValue stats(UniValue::VOBJ);
    stats.pushKV("records", static_cast<uint64_t>(result.size()));
    stats.pushKV("total_amount", ValueFromAmount(totalAmount));
    retObj.pushKV("tx", result);
    retObj.pushKV("collated", stats);
    return retObj;
  }

  return result;
}

// clang-format off
static const CRPCCommand commands[] = {
//  category               name                      actor (function)         argNames
//  ---------------------  ------------------------  -----------------------  ------------------------------------------
  {"wallet",             "sendtypeto",               &sendtypeto,             {"typein", "typeout", "outputs", "comment", "comment_to", "test_fee", "coincontrol"}},
  {"wallet",             "stakeat",                  &stakeat,                {"recipient", "test_fee", "coincontrol"}},
  {"wallet",             "filtertransactions",       &filtertransactions,     {"options"}},
};
// clang-format on

void RegisterWalletextRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
