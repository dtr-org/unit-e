#include <wallet/rpcvalidator.h>

#include <esperanza/finalizationstate.h>
#include <esperanza/validatorstate.h>
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
            + HelpExampleRpc("deposit", ""));
  }

  pwallet->BlockUntilSyncedToCurrentChain();

  if (!extWallet.nIsValidatorEnabled){
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Validating is disabled.");
  }

  UniValue result(UniValue::VOBJ);

  CTxDestination address = DecodeDestination(request.params[0].get_str());
  if (!IsValidDestination(address)) {
    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
  }

  CAmount amount = AmountFromValue(request.params[1]);

  if (extWallet.validatorState.m_phase == +esperanza::ValidatorState::Phase::IS_VALIDATING){
    throw JSONRPCError(RPC_INVALID_PARAMETER, "The node is already validating.");
  }

  if (!esperanza::FinalizationState::ValidateDepositAmount(amount)) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount is below minimum allowed.");
  }

  CWalletTx tx;
  extWallet.SendDeposit(address, amount, tx);

  result.push_back(Pair("transactionid", tx.GetHash().GetHex()));

  return result;
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

  if (!extWallet.nIsValidatorEnabled) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Validating is disabled.");
  }

  UniValue result(UniValue::VOBJ);

  if (extWallet.validatorState.m_phase !=
      +esperanza::ValidatorState::Phase::IS_VALIDATING) {
    throw JSONRPCError(RPC_INVALID_PARAMETER,
                       "The node is not validating validating.");
  }

  CWalletTx tx;
  extWallet.SendLogout(tx);

  result.push_back(Pair("transactionid", tx.GetHash().GetHex()));

  return result;
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

  UniValue obj(UniValue::VOBJ);

  std::string status;

  switch (extWallet.validatorState.m_phase) {
    case esperanza::ValidatorState::Phase::NOT_VALIDATING:
      status = "NOT_VALIDATING";
      break;
    case esperanza::ValidatorState::Phase::WAITING_DEPOSIT_CONFIRMATION:
      status = "WAITING_DEPOSIT_CONFIRMATION";
      break;
    case esperanza::ValidatorState::Phase::WAITING_DEPOSIT_FINALIZATION:
      status = "WAITING_DEPOSIT_FINALIZATION";
      break;
    case esperanza::ValidatorState::Phase::IS_VALIDATING:
      status = "IS_VALIDATING";
      break;
    default: status = "UNKNOWN";
  }

  obj.pushKV("enabled", gArgs.GetBoolArg("-validating", true));
  obj.pushKV("validator_status", extWallet.validatorState.m_phase._to_string());

  return obj;
}

static const CRPCCommand commands[] =
    { //  category              name                        actor (function)           argNames
      //  --------------------- ------------------------    -----------------------  ----------
        { "wallet",             "deposit",                  &deposit,                  {"address", "amount"} },
        { "wallet",             "logout",                   &logout,                   {} },
        { "wallet",             "getvalidatorinfo",         &getvalidatorinfo,         {} },
    };

void RegisterValidatorRPCCommands(CRPCTable &t)
{
  for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
    t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
