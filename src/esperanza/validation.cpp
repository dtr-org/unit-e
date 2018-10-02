// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <esperanza/adminparams.h>
#include <esperanza/finalizationstate.h>
#include <esperanza/params.h>
#include <esperanza/validation.h>
#include <script/interpreter.h>
#include <script/standard.h>
#include <util.h>
#include <validation.h>

namespace esperanza {

bool CheckDepositTransaction(CValidationState &errState, const CTransaction &tx,
                             const CBlockIndex *pindex) {
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

  FinalizationState *state = FinalizationState::GetState(pindex);

  esperanza::Result res = state->ValidateDeposit(
      CPubKey(vSolutions[0]).GetHash(), tx.GetValueOut());

  if (res != +esperanza::Result::SUCCESS) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-deposit-invalid-esperanza");
  }

  return true;
}

//! \brief Check if the vote is referring to an epoch before the last known
//! finalization. This should be safe since finalization should prevent reorgs.
//! It assumes that the vote is well formed and in general parsable. It does not
//! make anycheck over the validity of the vote transaction.
//! \param tx transaction containing the vote.
//! \returns true if the vote is expired, false otherwise.
bool IsVoteExpired(const CTransaction &tx) {

  Vote vote = CScript::ExtractVoteFromSignature(tx.vin[0].scriptSig);
  FinalizationState *state = FinalizationState::GetState();

  return vote.m_targetEpoch <= state->GetLastFinalizedEpoch();
}

bool CheckLogoutTransaction(CValidationState &errState, const CTransaction &tx,
                            const Consensus::Params &consensusParams,
                            const CBlockIndex *pindex) {

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

  esperanza::FinalizationState *state =
      esperanza::FinalizationState::GetState(pindex);

  esperanza::Result res =
      state->ValidateLogout(CPubKey(vSolutions[0]).GetHash());

  if (res != +esperanza::Result::SUCCESS) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-logout-invalid-esperanza");
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
                              const CBlockIndex *pindex) {

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

  esperanza::FinalizationState *state =
      esperanza::FinalizationState::GetState(pindex);

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

  esperanza::Result res = state->ValidateWithdraw(
      CPubKey(prevSolutions[0]).GetHash(), tx.vout[0].nValue);

  if (res != +esperanza::Result::SUCCESS) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-withdraw-invalid-esperanza");
  }

  if (!prevTx->IsLogout() && !prevTx->IsVote()) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-withdraw-prev-not-logout-or-vote");
  }

  return true;
}

bool CheckVoteTransaction(CValidationState &errState, const CTransaction &tx,
                          const Consensus::Params &consensusParams,
                          const CBlockIndex *pindex) {

  if (tx.vin.size() != 1 || tx.vout.size() != 1) {
    return errState.DoS(10, false, REJECT_INVALID, "bad-vote-malformed");
  }

  if (!IsPayVoteSlashScript(tx.vout[0].scriptPubKey)) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-vote-vout-script-invalid-payvoteslash");
  }

  FinalizationState *state = FinalizationState::GetState(pindex);

  esperanza::Result res = state->ValidateVote(
      CScript::ExtractVoteFromSignature(tx.vin[0].scriptSig));

  if (res != +esperanza::Result::SUCCESS) {
    return errState.DoS(10, false, REJECT_INVALID,
                        "bad-vote-invalid-esperanza");
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

bool CheckAdminTransaction(CValidationState &state, const CTransaction &tx,
                           const CBlockIndex *pindex) {
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
    if (!DecodeAdminCommand(out.scriptPubKey, command)) {
      continue;
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

  const esperanza::FinalizationState *const finalizationState =
      esperanza::FinalizationState::GetState(pindex);

  AdminKeySet set;
  std::copy_n(keys.begin(), ADMIN_MULTISIG_KEYS, set.begin());
  const auto result = finalizationState->ValidateAdminKeys(set);

  if (result != +Result::SUCCESS) {
    return state.DoS(10, false, REJECT_INVALID, "admin-not-authorized");
  }

  return true;
}

}  // namespace esperanza
