// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/rpcvalidator.h>

#include <core_io.h>
#include <esperanza/finalizationstate.h>
#include <esperanza/validatorstate.h>
#include <injector.h>
#include <key_io.h>
#include <rpc/server.h>
#include <validation.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>


UniValue deposit(const JSONRPCRequest &request)
{
  const std::shared_ptr<CWallet> wallet = GetWalletForJSONRPCRequest(request);
  CWallet* const pwallet = wallet.get();
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

  {
    LOCK(GetComponent<finalization::StateRepository>()->GetLock());
    const finalization::FinalizationState *fin_state =
      GetComponent<finalization::StateRepository>()->GetTipState();
    assert(fin_state != nullptr);

    if (!fin_state->ValidateDepositAmount(amount)) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount is below minimum allowed.");
    }
  }

  CTransactionRef tx;
  if (!extWallet.SendDeposit(*keyID, amount, tx)) {
    throw JSONRPCError(RPC_TRANSACTION_ERROR, "Cannot create deposit.");
  }

  return tx->GetHash().GetHex();
}

UniValue withdraw(const JSONRPCRequest &request)
{
  const std::shared_ptr<CWallet> wallet = GetWalletForJSONRPCRequest(request);
  CWallet* const pwallet = wallet.get();

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

  CTransactionRef tx;
  if (!extWallet.SendWithdraw(address, tx)) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot send withdraw transaction.");
  }

  return tx->GetHash().GetHex();
}

UniValue logout(const JSONRPCRequest& request) {
  const std::shared_ptr<CWallet> wallet = GetWalletForJSONRPCRequest(request);
  CWallet* const pwallet = wallet.get();

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

  CTransactionRef tx;

  if (!extWallet.SendLogout(tx)) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot send logout transaction.");
  }

  return tx->GetHash().GetHex();
}

UniValue getvalidatorinfo(const JSONRPCRequest &request){
  const std::shared_ptr<CWallet> wallet = GetWalletForJSONRPCRequest(request);
  CWallet* const pwallet = wallet.get();

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

UniValue createvotetransaction(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() != 2) {
    throw std::runtime_error(
      "createvotetransaction\n"
      "\nReturns raw transaction data\n"
      "\nArguments:\n"
      "1.\n"
      "{\n"
      "  \"validator_address\": xxxx   (string) the validator address\n"
      "  \"target_hash\": xxxx        (string) the target hash\n"
      "  \"source_epoch\": xxxx       (numeric) the source epoch\n"
      "  \"target_epoch\": xxxx       (numeric) the target epoch\n"
      "}\n"
      "2. prev_tx                     (string) previous transaction hash\n"
      "Result: raw transaction\n"
      "\n"
      + HelpExampleCli("createvotetransaction", "{\"validator_address\": xxxx, \"target_hash\": xxxx, \"source_epoch\": xxxx, \"target_epoch\": xxxx} txid")
      + HelpExampleRpc("createvotetransaction", "{\"validator_address\": xxxx, \"target_hash\": xxxx, \"source_epoch\": xxxx, \"target_epoch\": xxxx} txid"));
  }

  std::shared_ptr<CWallet> wallet = GetWalletForJSONRPCRequest(request);
  CWallet* const pwallet = wallet.get();
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)){
    return NullUniValue;
  }

  esperanza::Vote vote;

  UniValue v = request.params[0].get_obj();
  vote.m_validator_address = ParseHash160O(v, "validator_address");
  vote.m_target_hash = ParseHashO(v, "target_hash");
  vote.m_source_epoch = find_value(v, "source_epoch").get_int();
  vote.m_target_epoch = find_value(v, "target_epoch").get_int();

  CTransactionRef prev_tx;
  uint256 txid = ParseHashV(request.params[1], "txid");
  uint256 hash_block;
  CBlockIndex *block_index = nullptr;
  if (!GetTransaction(txid, prev_tx, Params().GetConsensus(), hash_block, true, block_index)) {
    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No transaction with such id");
  }

  const CScript &script_pubkey = prev_tx->vout[0].scriptPubKey;
  const CAmount amount = prev_tx->vout[0].nValue;

  std::vector<unsigned char> vote_sig;
  if (!esperanza::Vote::CreateSignature(pwallet, vote, vote_sig)) {
    throw JSONRPCError(RPC_WALLET_ERROR, "Cannot sign vote");
  }

  CScript script_sig = CScript::EncodeVote(vote, vote_sig);

  CMutableTransaction tx;
  tx.SetType(TxType::VOTE);
  tx.vin.push_back(
    CTxIn(prev_tx->GetHash(), 0, script_sig, CTxIn::SEQUENCE_FINAL));

  CTxOut txout(amount, script_pubkey);
  tx.vout.push_back(txout);

  return EncodeHexTx(tx, RPCSerializationFlags());
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category              name                        actor (function)           argNames
  //  --------------------- ------------------------    -----------------------  ----------
    { "wallet",             "deposit",                  &deposit,                  {"address", "amount"} },
    { "wallet",             "logout",                   &logout,                   {} },
    { "wallet",             "withdraw",                 &withdraw,                 {"address"} },
    { "wallet",             "getvalidatorinfo",         &getvalidatorinfo,         {} },
    { "wallet",             "createvotetransaction",    &createvotetransaction,    {"vote", "txid"}},
};
// clang-format on

void RegisterValidatorRPCCommands(CRPCTable &t)
{
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
