#include <wallet/rpcadmin.h>

#include <base58.h>
#include <consensus/validation.h>
#include <esperanza/admincommand.h>
#include <net.h>
#include <rpc/server.h>
#include <wallet/coincontrol.h>
#include <utility>

struct InputData {
  COutPoint m_outPoint;
  CTxOut m_txOut;

  InputData(const COutPoint &outPoint, CTxOut txOut)
      : m_outPoint(outPoint), m_txOut(std::move(txOut)) {}
};

CWalletTx SignAndSend(CMutableTransaction &&mutableTx, CWallet *const wallet,
                      const std::vector<InputData> &inputDatas) {
  CTransaction constTx(mutableTx);
  SignatureData sigdata;

  for (size_t i = 0; i < constTx.vin.size(); ++i) {
    const auto data = inputDatas[i];
    const auto &scriptPubKey = data.m_txOut.scriptPubKey;
    const auto amountIn = data.m_txOut.nValue;

    if (!ProduceSignature(TransactionSignatureCreator(wallet, &constTx, i,
                                                      amountIn, SIGHASH_ALL),
                          scriptPubKey, sigdata, &constTx)) {
      throw JSONRPCError(RPC_TRANSACTION_ERROR, "Unable to sign transaction");
    }

    UpdateTransaction(mutableTx, i, sigdata);
  }

  CWalletTx walletTx;
  walletTx.fTimeReceivedIsTxTime = true;
  walletTx.BindWallet(wallet);
  walletTx.SetTx(MakeTransactionRef(std::move(mutableTx)));

  CReserveKey reservekey(wallet);
  CValidationState state;
  if (!wallet->CommitTransaction(walletTx, reservekey, g_connman.get(),
                                 state)) {
    throw JSONRPCError(RPC_TRANSACTION_ERROR, "Unable to commit transaction");
  }

  if (state.IsInvalid()) {
    const auto &reason = state.GetRejectReason();
    throw JSONRPCError(RPC_VERIFY_REJECTED,
                       "Unable to validate transaction: " + reason);
  }

  return walletTx;
}

std::vector<CPubKey> ParsePayload(const UniValue &value) {
  std::vector<CPubKey> pubkeys;
  for (size_t i = 0; i < value.size(); ++i) {
    const auto &keyStr = value[i].get_str();
    std::vector<uint8_t> buffer;

    if (keyStr.size() == 2 * CPubKey::COMPRESSED_PUBLIC_KEY_SIZE) {
      buffer = ParseHex(keyStr);
    } else if (!DecodeBase58(keyStr, buffer)) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid pubkey");
    }

    pubkeys.emplace_back(buffer.begin(), buffer.end());
  }

  return pubkeys;
}

std::vector<esperanza::AdminCommand> ParseCommands(const UniValue &value) {
  std::vector<esperanza::AdminCommand> commands;

  for (size_t i = 0; i < value.size(); ++i) {
    const auto &command = value[i];
    const auto &commandName = command["cmd"].get_str();

    if (commandName == "end_permissioning") {
      commands.emplace_back(esperanza::AdminCommand(
          esperanza::AdminCommandType::END_PERMISSIONING, {}));
      continue;
    }

    const auto &payload = ParsePayload(command["payload"]);

    if (commandName == "whitelist") {
      commands.emplace_back(esperanza::AdminCommandType::ADD_TO_WHITELIST,
                            payload);
    } else if (commandName == "blacklist") {
      commands.emplace_back(esperanza::AdminCommandType::REMOVE_FROM_WHITELIST,
                            payload);
    } else if (commandName == "reset_admins") {
      commands.emplace_back(esperanza::AdminCommandType::RESET_ADMINS, payload);
    } else {
      throw JSONRPCError(RPC_INVALID_PARAMETER,
                         "Unknown command: " + commandName);
    }
  }

  return commands;
}

std::vector<InputData> GetInputsData(CWallet *const wallet,
                                     const std::vector<COutPoint> &outPoints) {
  std::vector<InputData> inputs;
  for (const auto outPoint : outPoints) {
    const auto it = wallet->mapWallet.find(outPoint.hash);
    if (it == wallet->mapWallet.end()) {
      throw JSONRPCError(RPC_INVALID_PARAMETER,
                         "Can't find prevout transaction hash");
    }

    auto const tx = it->second.tx;
    inputs.emplace_back(outPoint, tx->vout[outPoint.n]);
  }

  return inputs;
}

std::vector<COutPoint> ParseOutPoints(const UniValue &node) {
  std::vector<COutPoint> outs;
  for (size_t i = 0; i < node.size(); ++i) {
    const auto &tuple = node[i];
    const auto hash = ParseHashV(tuple[0], "prevoutHash");
    const auto index = static_cast<uint32_t>(tuple[1].get_int64());

    outs.emplace_back(hash, index);
  }

  return outs;
}

UniValue sendadmincommands(const JSONRPCRequest &request) {
  CWallet *const wallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(wallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() < 3 || request.params.size() > 4) {
    throw std::runtime_error(
        "sendadmincommands\n"
        "Sends admin commands in a single transaction."
        "\nArguments:\n"
        "1. prevouts    (required) input UTXOs. [(tx_hash, out_n)ъ.\n"
        "2. fee         (required) fee you want to pay for this transaction.\n"
        "3. commands    (required) list of commands to send:\n"
        "                          {'cmd': 'end_permissioning'}\n"
        "                          {'cmd': 'whitelist', 'payload': <keys>}\n"
        "                          {'cmd': 'blacklist', 'payload': <keys>}\n"
        "                          {'cmd': 'reset_admins', 'payload': <keys>}\n"
        "4. destination (optional) where to send change if any.\n"
        "\nExamples:\n" +
        HelpExampleRpc("sendadmincommands", ""));
  }

  wallet->BlockUntilSyncedToCurrentChain();

  const auto prevoutPoints = ParseOutPoints(request.params[0]);
  const CAmount desiredFee = AmountFromValue(request.params[1]);
  const auto commands = ParseCommands(request.params[2]);
  CTxDestination remainderDestination;

  if (!request.params[3].isNull()) {
    remainderDestination = DecodeDestination(request.params[3].get_str());
  }

  CMutableTransaction adminTx;
  adminTx.SetType(+TxType::ADMIN);

  const auto inputsData = GetInputsData(wallet, prevoutPoints);
  CAmount totalAmountInInputs = 0;

  for (const auto &inputData : inputsData) {
    adminTx.vin.emplace_back(inputData.m_outPoint.hash, inputData.m_outPoint.n);
    totalAmountInInputs += inputData.m_txOut.nValue;
  }

  for (const auto &command : commands) {
    adminTx.vout.emplace_back(0, EncodeAdminCommand(command));
  }

  if (totalAmountInInputs < desiredFee) {
    throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
                       "Account has insufficient funds");
  }

  if (totalAmountInInputs > desiredFee) {
    if (!IsValidDestination(remainderDestination)) {
      throw JSONRPCError(RPC_INVALID_PARAMETER,
                         "No remainder destination provided");
    }

    const CAmount remainder = totalAmountInInputs - desiredFee;
    const auto &scriptPubKey = GetScriptForDestination(remainderDestination);
    adminTx.vout.emplace_back(remainder, scriptPubKey);
  }

  const auto walletTx = SignAndSend(std::move(adminTx), wallet, inputsData);

  return walletTx.GetHash().GetHex();
}

// clang-format off
static const CRPCCommand commands[] =
    {  //  category name                actor (function)   argNames
       //  ------- -------------------- ------------------- ---------------------
        {"wallet", "sendadmincommands", &sendadmincommands, {"prevouts", "fee", "commands", "destination"}}};
// clang-format on

void RegisterAdminRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) t.appendCommand(command.name, &command);
}
