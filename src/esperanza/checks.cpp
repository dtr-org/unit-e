// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <esperanza/adminparams.h>
#include <esperanza/checks.h>
#include <esperanza/finalizationstate.h>
#include <finalization/vote_recorder.h>
#include <script/interpreter.h>
#include <script/standard.h>
#include <util.h>
#include <validation.h>

namespace esperanza {

bool CheckFinalizationTransaction(const CTransaction &tx, CValidationState &err_state,
                                  const Consensus::Params &params, const FinalizationState &fin_state) {
  switch (tx.GetType()) {
    case +TxType::STANDARD:
    case +TxType::COINBASE:
      assert(not("Shouldn't be called on non-finalization transaction"));
    case +TxType::DEPOSIT:
      return CheckDepositTransaction(err_state, tx, fin_state);
    case +TxType::VOTE:
      return CheckVoteTransaction(err_state, tx, params, fin_state);
    case +TxType::LOGOUT:
      return CheckLogoutTransaction(err_state, tx, params, fin_state);
    case +TxType::SLASH:
      return CheckSlashTransaction(err_state, tx, params, fin_state);
    case +TxType::WITHDRAW:
      return CheckWithdrawTransaction(err_state, tx, params, fin_state);
    case +TxType::ADMIN:
      return CheckAdminTransaction(err_state, tx, fin_state);
  }
  return false;
}

bool CheckDepositTransaction(CValidationState &errState, const CTransaction &tx,
                             const FinalizationState &state) {

  assert(tx.IsDeposit());

  if (tx.vin.empty() || tx.vout.empty()) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-deposit-malformed");
  }

  if (!IsPayVoteSlashScript(tx.vout[0].scriptPubKey)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-deposit-vout-script-invalid-payvoteslash");
  }

  std::vector<std::vector<unsigned char>> vSolutions;
  txnouttype typeRet;
  if (!Solver(tx.vout[0].scriptPubKey, typeRet, vSolutions)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-deposit-script-not-solvable");
  }

  uint160 validatorAddress = uint160();

  if (!ExtractValidatorAddress(tx, validatorAddress)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-deposit-cannot-extract-validator-address");
  }

  const Result res = state.ValidateDeposit(validatorAddress, tx.GetValueOut());

  if (res != +Result::SUCCESS) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-deposit-invalid-state");
  }

  return true;
}

bool IsVoteExpired(const CTransaction &tx) {

  assert(tx.IsVote());

  Vote vote;
  std::vector<unsigned char> voteSig;
  assert(CScript::ExtractVoteFromVoteSignature(tx.vin[0].scriptSig, vote,
                                               voteSig));
  const FinalizationState *state = FinalizationState::GetState();

  return vote.m_targetEpoch < state->GetCurrentEpoch();
}

bool CheckLogoutTransaction(CValidationState &errState, const CTransaction &tx,
                            const Consensus::Params &consensusParams,
                            const FinalizationState &state) {

  assert(tx.IsLogout());

  if (tx.vin.size() != 1 || tx.vout.size() != 1) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-logout-malformed");
  }

  if (!IsPayVoteSlashScript(tx.vout[0].scriptPubKey)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-logout-vout-script-invalid-payvoteslash");
  }

  std::vector<std::vector<unsigned char>> vSolutions;
  txnouttype typeRet;
  if (!Solver(tx.vout[0].scriptPubKey, typeRet, vSolutions)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-logout-script-not-solvable");
  }

  uint160 validatorAddress = uint160();

  if (!ExtractValidatorAddress(tx, validatorAddress)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-logout-cannot-extract-validator-address");
  }

  const Result res = state.ValidateLogout(validatorAddress);

  if (res != +Result::SUCCESS) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-logout-invalid-state");
  }

  // We keep the check for the prev at the end because is the most expensive
  // check (potentially goes to disk) and there is a good chance that if the
  // vote is not valid (i.e. outdated) then the function will return before
  // reaching this point.
  CTransactionRef prevTx;
  uint256 blockHash;

  // We have to look into the tx database to find the prev tx, hence the
  // use of fAllowSlow = true
  if (!GetTransaction(tx.vin[0].prevout.hash, prevTx, consensusParams,
                      blockHash, true)) {

    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-logout-no-prev-tx-found");
  }

  if (!prevTx->IsDeposit() && !prevTx->IsVote()) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-logout-prev-not-deposit-or-vote");
  }

  if (prevTx->vout[0].scriptPubKey != tx.vout[0].scriptPubKey) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-logout-not-same-payvoteslash-script");
  }

  return true;
}

bool CheckWithdrawTransaction(CValidationState &errState,
                              const CTransaction &tx,
                              const Consensus::Params &consensusParams,
                              const FinalizationState &state) {

  assert(tx.IsWithdraw());

  if (tx.vin.size() != 1 || tx.vout.size() > 3) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-withdraw-malformed");
  }

  if (!tx.vout[0].scriptPubKey.IsPayToPublicKeyHash()) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-withdraw-vout-script-invalid-p2pkh");
  }

  std::vector<std::vector<unsigned char>> vSolutions;
  txnouttype typeRet;
  if (!Solver(tx.vout[0].scriptPubKey, typeRet, vSolutions)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-withdraw-script-not-solvable");
  }

  CTransactionRef prevTx;
  uint256 blockHash;

  // We have to look into the tx database to find the prev tx, hence the
  // use of fAllowSlow = true
  if (!GetTransaction(tx.vin[0].prevout.hash, prevTx, consensusParams,
                      blockHash, true)) {

    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-withdraw-no-prev-tx-found");
  }

  std::vector<std::vector<unsigned char>> prevSolutions;
  txnouttype prevTypeRet;
  if (!Solver(prevTx->vout[0].scriptPubKey, prevTypeRet, prevSolutions)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-logout-script-not-solvable");
  }

  uint160 validatorAddress = uint160();

  if (!ExtractValidatorAddress(tx, validatorAddress)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-logout-cannot-extract-validator-address");
  }

  const Result res = state.ValidateWithdraw(validatorAddress, tx.vout[0].nValue);

  if (res != +Result::SUCCESS) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-withdraw-invalid-state");
  }

  if (!prevTx->IsLogout() && !prevTx->IsVote()) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-withdraw-prev-not-logout-or-vote");
  }

  return true;
}

bool CheckVoteTransaction(CValidationState &errState, const CTransaction &tx,
                          const Consensus::Params &consensusParams,
                          const FinalizationState &state) {

  assert(tx.IsVote());

  if (tx.vin.size() != 1 || tx.vout.size() != 1) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-vote-malformed");
  }

  if (!IsPayVoteSlashScript(tx.vout[0].scriptPubKey)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-vote-vout-script-invalid-payvoteslash");
  }

  Vote vote;
  std::vector<unsigned char> voteSig;
  if (!CScript::ExtractVoteFromVoteSignature(tx.vin[0].scriptSig, vote,
                                             voteSig)) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-vote-data-format");
  }

  CPubKey pubkey;
  if (!ExtractValidatorPubkey(tx, pubkey)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-scriptpubkey-pubkey-format");
  }

  if (!esperanza::Vote::CheckSignature(pubkey, vote, voteSig)) {
    return errState.DoS(100, false, REJECT_INVALID, "bad-vote-signature");
  }

  if (state.ValidateVote(vote) != +Result::SUCCESS) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-vote-invalid-state");
  }

  // We keep the check for the prev at the end because is the most expensive
  // check (potentially goes to disk) and there is a good chance that if the
  // vote is not valid (i.e. outdated) then the function will return before
  // reaching this point.
  CTransactionRef prevTx;
  uint256 blockHash;
  // We have to look into the tx database to find the prev tx, hence the
  // use of fAllowSlow = true
  if (!GetTransaction(tx.vin[0].prevout.hash, prevTx, consensusParams,
                      blockHash, true)) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-vote-no-prev-tx-found");
  }

  if (!prevTx->IsDeposit() && !prevTx->IsVote() && !prevTx->IsLogout()) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-vote-prev-not-deposit-vote-or-logout");
  }

  if (prevTx->vout[0].scriptPubKey != tx.vout[0].scriptPubKey) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-vote-not-same-payvoteslash-script");
  }

  return true;
}

bool CheckSlashTransaction(CValidationState &errState, const CTransaction &tx,
                           const Consensus::Params &consensusParams,
                           const FinalizationState &state) {

  assert(tx.IsSlash());

  if (tx.vin.size() != 1 || tx.vout.size() != 1) {
    return errState.DoS(100, false, REJECT_INVALID, "bad-slash-malformed");
  }

  Vote vote1;
  Vote vote2;
  std::vector<unsigned char> vote1Sig;
  std::vector<unsigned char> vote2Sig;
  if (!CScript::ExtractVotesFromSlashSignature(tx.vin[0].scriptSig, vote1,
                                               vote2, vote1Sig, vote2Sig)) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-slash-data-format");
  }

  const esperanza::Result res = state.IsSlashable(vote1, vote2);

  if (res != +esperanza::Result::SUCCESS) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-slash-not-slashable");
  }

  return true;
}

bool CheckAdminTransaction(CValidationState &state, const CTransaction &tx,
                           const FinalizationState &finalizationState) {

  if (!finalizationState.IsPermissioningActive()) {
    return state.DoS(10, false, REJECT_INVALID, "admin-disabled");
  }

  if (tx.vin.empty()) {
    return state.DoS(10, false, REJECT_INVALID, "admin-vin-empty");
  }

  if (tx.vout.empty()) {
    return state.DoS(10, false, REJECT_INVALID, "admin-vout-empty");
  }

  size_t validCommandsNum = 0;
  bool disablesPermissioning = false;

  for (const auto &out : tx.vout) {
    AdminCommand command;
    if (!MatchAdminCommand(out.scriptPubKey)) {
      continue;
    }

    if (!DecodeAdminCommand(out.scriptPubKey, command)) {
      return state.DoS(10, false, REJECT_INVALID, "admin-invalid-command");
    }

    if (disablesPermissioning) {
      return state.DoS(10, false, REJECT_INVALID, "admin-double-disable");
    }

    if (command.GetCommandType() ==
        +esperanza::AdminCommandType::END_PERMISSIONING) {
      disablesPermissioning = true;
    }

    ++validCommandsNum;
  }

  if (validCommandsNum == 0) {
    return state.DoS(10, false, REJECT_INVALID, "admin-no-commands");
  }

  const auto &witness = tx.vin.front().scriptWitness;

  std::vector<CPubKey> keys;

  if (witness.stack.size() != ADMIN_MULTISIG_SIGNATURES + 2 ||
      !CScript::ExtractAdminKeysFromWitness(witness, keys) ||
      keys.size() != ADMIN_MULTISIG_KEYS) {
    return state.DoS(10, false, REJECT_INVALID, "admin-invalid-witness");
  }

  AdminKeySet set;
  std::copy_n(keys.begin(), ADMIN_MULTISIG_KEYS, set.begin());
  const auto result = finalizationState.ValidateAdminKeys(set);

  if (result != +Result::SUCCESS) {
    return state.DoS(10, false, REJECT_INVALID, "admin-not-authorized");
  }

  return true;
}

bool ExtractValidatorPubkey(const CTransaction &tx, CPubKey &pubkeyOut) {
  if (tx.IsVote()) {

    std::vector<std::vector<unsigned char>> vSolutions;
    txnouttype typeRet;

    if (Solver(tx.vout[0].scriptPubKey, typeRet, vSolutions)) {
      pubkeyOut = CPubKey(vSolutions[0]);
      return true;
    }
  }
  return false;
}

bool ExtractValidatorAddress(const CTransaction &tx,
                             uint160 &validatorAddressOut) {

  switch (tx.GetType()) {
    case TxType::DEPOSIT:
    case TxType::LOGOUT: {
      std::vector<std::vector<unsigned char>> vSolutions;
      txnouttype typeRet;

      if (Solver(tx.vout[0].scriptPubKey, typeRet, vSolutions)) {
        assert(typeRet == TX_PAYVOTESLASH);
        validatorAddressOut = CPubKey(vSolutions[0]).GetID();
        return true;
      }
      return false;
    }
    case TxType::WITHDRAW: {

      const CScript scriptSig = tx.vin[0].scriptSig;
      auto pc = scriptSig.begin();
      std::vector<unsigned char> vData;
      opcodetype opcode;

      // Skip the first value (signature)
      scriptSig.GetOp(pc, opcode);

      // Retrieve the public key
      scriptSig.GetOp(pc, opcode, vData);
      validatorAddressOut = CPubKey(vData).GetID();
      return true;
    }
    default: {
      return false;
    }
  }
}

}  // namespace esperanza
