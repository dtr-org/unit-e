// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/rpcvalidator.h>

#include <esperanza/finalizationstate.h>
#include <esperanza/validatorstate.h>
#include <injector.h>
#include <rpc/server.h>
#include <rpc/safemode.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>


UniValue deposit(const JSONRPCRequest &request)
{

  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)){
    return NullUniValue;
  }

  esperanza::WalletExtension& extWallet = pwallet->GetWalletExtension();

  if (request.fHelp || request.params.size() != 2) {

    throw std::runtime_error(
        "deposit\n"
        "Creates a new deposit of the given amount, if accepted it will make the current node a validator."
        "\nArguments:\n"
        "1. address              (required) the destination for the deposit.\n"
        "2. amount               (required) the amount deposit.\n"
        "\nExamples:\n"
            + HelpExampleRpc("deposit", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" 150000000000"));
  }

  pwallet->BlockUntilSyncedToCurrentChain();

  if (!extWallet.nIsValidatorEnabled || !extWallet.validatorState) {
    throw JSONRPCError(RPC_INVALID_REQUEST, "The node must be a validator.");
  }

  CTxDestination address = DecodeDestination(request.params[0].get_str());
  if (!IsValidDestination(address)) {
    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address.");
  }
  CKeyID *keyID = boost::get<CKeyID>(&address);

  if (keyID == nullptr) {
    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Address must be a P2PKH address.");
  }

  CAmount amount = AmountFromValue(request.params[1]);

  esperanza::ValidatorState &validator = extWallet.validatorState.get();
  if (validator.m_phase == +esperanza::ValidatorState::Phase::IS_VALIDATING){
    throw JSONRPCError(RPC_INVALID_PARAMETER, "The node is already validating.");
  }

  const finalization::FinalizationState *state =
    GetComponent<finalization::StateRepository>()->GetTipState();
  assert(state != nullptr);

  if (!state->ValidateDepositAmount(amount)) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount is below minimum allowed.");
  }

  CWalletTx tx;
  if (!extWallet.SendDeposit(*keyID, amount, tx)) {
    throw JSONRPCError(RPC_TRANSACTION_ERROR, "Cannot create deposit.");
  }

  return tx.GetHash().GetHex();
}

UniValue withdraw(const JSONRPCRequest &request)
{

  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)){
    return NullUniValue;
  }

  esperanza::WalletExtension& extWallet = pwallet->GetWalletExtension();

  if (request.fHelp || request.params.size() != 1) {

    throw std::runtime_error(
        "withdraw\n"
        "Withdraw all funds form the validator's deposit and makes them available for the given address."
        "\nArguments:\n"
        "1. address              (required) the destination for the withdrawn funds.\n"
        "\nExamples:\n"
            + HelpExampleRpc("withdraw", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\""));
  }

  pwallet->BlockUntilSyncedToCurrentChain();

  if (!extWallet.nIsValidatorEnabled || !extWallet.validatorState){
    throw JSONRPCError(RPC_INVALID_REQUEST, "The node must be a validator.");
  }

  CTxDestination address = DecodeDestination(request.params[0].get_str());
  if (!IsValidDestination(address)) {
    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
  }

  esperanza::ValidatorState &validator = extWallet.validatorState.get();
  if(validator.m_last_esperanza_tx == nullptr) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Not a validator.");
  }

  if(validator.m_last_esperanza_tx->IsWithdraw()) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Already withdrawn.");
  }

  if (validator.m_phase != +esperanza::ValidatorState::Phase::NOT_VALIDATING){
    throw JSONRPCError(RPC_INVALID_PARAMETER, "The node is validating, logout first.");
  }

  CWalletTx tx;
  if (!extWallet.SendWithdraw(address, tx)) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot send withdraw transaction.");
  }

  return tx.GetHash().GetHex();
}

UniValue logout(const JSONRPCRequest& request) {

  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
    return NullUniValue;
  }

  esperanza::WalletExtension& extWallet = pwallet->GetWalletExtension();

  if (request.fHelp || request.params.size() != 0) {

    throw std::runtime_error(
        "logout\n"
        "Creates a logout request, if accepted it will start the logout "
        "process for the validator."
        "\nExamples:\n" +
            HelpExampleRpc("logout", ""));
  }

  pwallet->BlockUntilSyncedToCurrentChain();

  if (!extWallet.nIsValidatorEnabled || !extWallet.validatorState){
    throw JSONRPCError(RPC_INVALID_REQUEST, "The node must be a validator.");
  }

  esperanza::ValidatorState &validator = extWallet.validatorState.get();
  if (validator.m_phase !=
      +esperanza::ValidatorState::Phase::IS_VALIDATING) {
    throw JSONRPCError(RPC_INVALID_PARAMETER,
                       "The node is not validating.");
  }

  CWalletTx tx;

  if (!extWallet.SendLogout(tx)) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot send logout transaction.");
  }

  return tx.GetHash().GetHex();
}

UniValue getvalidatorinfo(const JSONRPCRequest &request){

  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)){
    return NullUniValue;
  }

  esperanza::WalletExtension extWallet = pwallet->GetWalletExtension();

  if (request.fHelp || !request.params.empty())
    throw std::runtime_error(
        "getvalidatorinfo\n"
        "Returns an object containing validator-related information."
        "\nResult:\n"
        "{\n"
        "  \"enabled\": true|false,    (boolean) if staking is enabled or not on this wallet.\n"
        "  \"validator_status\":       (string) the current status of the validator.\n"
        "}\n"
        "\nExamples:\n"
            + HelpExampleCli("getvalidatorinfo", "")
            + HelpExampleRpc("getvalidatorinfo", ""));

  ObserveSafeMode();

  pwallet->BlockUntilSyncedToCurrentChain();

  if (!extWallet.nIsValidatorEnabled || !extWallet.validatorState) {
    throw JSONRPCError(RPC_INVALID_REQUEST, "The node must be a validator.");
  }

  esperanza::ValidatorState &validator = extWallet.validatorState.get();
  UniValue obj(UniValue::VOBJ);

  obj.pushKV("enabled", gArgs.GetBoolArg("-validating", true));
  obj.pushKV("validator_status", validator.m_phase._to_string());

  return obj;
}

static const CRPCCommand commands[] =
    { //  category              name                        actor (function)           argNames
      //  --------------------- ------------------------    -----------------------  ----------
        { "wallet",             "deposit",                  &deposit,                  {"address", "amount"} },
        { "wallet",             "logout",                   &logout,                   {} },
        { "wallet",             "withdraw",                 &withdraw,                 {"address"} },
        { "wallet",             "getvalidatorinfo",         &getvalidatorinfo,         {} },
    };

void RegisterValidatorRPCCommands(CRPCTable &t)
{
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
