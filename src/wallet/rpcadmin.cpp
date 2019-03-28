// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/rpcadmin.h>

#include <key_io.h>
#include <consensus/validation.h>
#include <esperanza/admincommand.h>
#include <net.h>
#include <rpc/server.h>
#include <wallet/coincontrol.h>
#include <utility>

struct UTXO {
  COutPoint m_outPoint;
  CTxOut m_txOut;

  UTXO(const COutPoint &outPoint, CTxOut txOut)
      : m_outPoint(outPoint), m_txOut(std::move(txOut)) {}
};

uint256 SignAndSend(CMutableTransaction &&mutableTx, CWallet *const wallet,
                      const std::vector<UTXO> &adminUTXOs) {
  CTransaction constTx = mutableTx;
  SignatureData sigdata;

  for (size_t i = 0; i < constTx.vin.size(); ++i) {
    const auto &utxo = adminUTXOs[i];
    const auto &scriptPubKey = utxo.m_txOut.scriptPubKey;
    const auto amountIn = utxo.m_txOut.nValue;

    const MutableTransactionSignatureCreator signatureCreator(
        &mutableTx, static_cast<unsigned>(i), amountIn, SIGHASH_ALL);

    if (!ProduceSignature(*wallet, signatureCreator, scriptPubKey, sigdata,
                          &constTx)) {
      LogPrint(BCLog::RPC, "Unable to sign admin transaction");
      throw JSONRPCError(RPC_TRANSACTION_ERROR,
                         "Unable to sign admin transaction");
    }

    UpdateInput(mutableTx.vin.at(i), sigdata);
  }

  CTransactionRef txref = MakeTransactionRef(mutableTx);
  CReserveKey reserveKey(wallet);
  CValidationState state;
  if (!wallet->CommitTransaction(txref, {} /* mapValue */, {} /* orderForm */, {} /* fromAccount */, reserveKey,
                                 g_connman.get(), state)) {
    LogPrint(BCLog::RPC, "Unable to commit admin transaction");
    throw JSONRPCError(RPC_TRANSACTION_ERROR,
                       "Unable to commit admin transaction");
  }

  if (state.IsInvalid()) {
    const auto &reason = state.GetRejectReason();
    LogPrint(BCLog::RPC, "Unable to validate admin transaction: %s", reason);
    throw JSONRPCError(RPC_VERIFY_REJECTED,
                       "Unable to validate admin transaction: " + reason);
  }

  return txref->GetHash();
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
    const auto &commandTypeStr = command["cmd"].get_str();

    const auto commandType = esperanza::AdminCommandType::_from_string_nothrow(
        command["cmd"].get_str().c_str());

    if (!commandType) {
      throw JSONRPCError(RPC_INVALID_PARAMETER,
                         "Unknown command: " + commandTypeStr);
    }

    const auto &payload = ParsePayload(command["payload"]);
    commands.emplace_back(commandType.value(), payload);
  }

  return commands;
}

std::vector<UTXO> GetAdminUTXOs(CWallet *const wallet, const UniValue &node) {
  std::vector<UTXO> utxos;
  for (size_t i = 0; i < node.size(); ++i) {
    const auto &tuple = node[i];
    const auto hash = ParseHashV(tuple[0], "prevoutHash");
    const auto index = static_cast<uint32_t>(tuple[1].get_int64());

    const auto it = wallet->mapWallet.find(hash);
    if (it == wallet->mapWallet.end()) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "Can't find admin utxo");
    }

    auto const tx = it->second.tx;
    utxos.emplace_back(COutPoint(hash, index), tx->vout[index]);
  }

  return utxos;
}

UniValue sendadmincommands(const JSONRPCRequest &request) {
  const std::shared_ptr<CWallet> wallet = GetWalletForJSONRPCRequest(request);
  CWallet * const pwallet = wallet.get();

  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
    return NullUniValue;
  }

  // clang-format off
  if (request.fHelp || request.params.size() < 3 || request.params.size() > 4) {
    throw std::runtime_error(
        "sendadmincommands\n"
        "Sends admin commands in a single transaction."
        "\nArguments:\n"
        "1. prevouts    (required) input UTXOs. [(tx_hash, out_n)ъ.\n"
        "2. fee         (required) fee you want to pay for this transaction.\n"
        "3. commands    (required) list of commands to send:\n"
        "                          {'cmd': 'END_PERMISSIONING'}\n"
        "                          {'cmd': 'ADD_TO_WHITELIST', 'payload': <keys>}\n"
        "                          {'cmd': 'REMOVE_FROM_WHITELIST', 'payload': <keys>}\n"
        "                          {'cmd': 'RESET_ADMINS', 'payload': <keys>}\n"
        "4. destination (optional) where to send change if any.\n"
        "\nExamples:\n" +
        HelpExampleRpc("sendadmincommands", ""));
  }
  // clang-format on

  wallet->BlockUntilSyncedToCurrentChain();

  const auto adminUTXOs = GetAdminUTXOs(pwallet, request.params[0]);
  const CAmount desiredFee = AmountFromValue(request.params[1]);
  const auto commands = ParseCommands(request.params[2]);
  CTxDestination remainderDestination;

  if (!request.params[3].isNull()) {
    remainderDestination = DecodeDestination(request.params[3].get_str());
  }

  CMutableTransaction adminTx;
  adminTx.SetType(+TxType::ADMIN);

  CAmount totalAmountInInputs = 0;

  for (const auto &utxo : adminUTXOs) {
    adminTx.vin.emplace_back(utxo.m_outPoint.hash, utxo.m_outPoint.n);
    totalAmountInInputs += utxo.m_txOut.nValue;
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

  const auto txhash = SignAndSend(std::move(adminTx), pwallet, adminUTXOs);

  return txhash.GetHex();
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
