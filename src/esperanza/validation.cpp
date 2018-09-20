// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <esperanza/finalizationstate.h>
#include <esperanza/params.h>
#include <esperanza/validation.h>
#include <script/interpreter.h>
#include <script/standard.h>
#include <util.h>
#include <validation.h>

namespace esperanza {

bool CheckDepositTransaction(CValidationState &state, const CTransaction &tx,
                             const CBlockIndex *pindex) {
  if (tx.vin.empty() || tx.vout.empty()) {
    return state.DoS(10, false, REJECT_INVALID, "bad-deposit-malformed");
  }

  if (!IsPayVoteSlashScript(tx.vout[0].scriptPubKey)) {
    return state.DoS(10, false, REJECT_INVALID,
                     "bad-deposit-vout-script-invalid-payvoteslash");
  }

  std::vector<std::vector<unsigned char>> vSolutions;
  txnouttype typeRet;
  if (!Solver(tx.vout[0].scriptPubKey, typeRet, vSolutions)) {
    return state.DoS(10, false, REJECT_INVALID,
                     "bad-deposit-script-not-solvable");
  }

  FinalizationState *esperanza = nullptr;
  if (pindex != nullptr) {
    esperanza = FinalizationState::GetState(*pindex);
  } else {
    esperanza = FinalizationState::GetState();
  }

  esperanza::Result res =
      esperanza->ValidateDeposit(tx.GetHash(), tx.GetValueOut());

  if (res != +esperanza::Result::SUCCESS) {
    return state.DoS(10, false, REJECT_INVALID,
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
  FinalizationState *esperanza = FinalizationState::GetState();

  return vote.m_targetEpoch <= esperanza->GetLastFinalizedEpoch();
}

bool CheckVoteTransaction(CValidationState &state, const CTransaction &tx,
                          const CBlockIndex *pindex) {
  if (tx.vin.size() != 1 || tx.vout.size() != 1) {
    return state.DoS(10, false, REJECT_INVALID, "bad-vote-malformed");
  }

  if (!IsPayVoteSlashScript(tx.vout[0].scriptPubKey)) {
    return state.DoS(10, false, REJECT_INVALID,
                     "bad-vote-vout-script-invalid-payvoteslash");
  }

  FinalizationState *esperanza = nullptr;
  if (pindex != nullptr) {
    esperanza = FinalizationState::GetState(*pindex);
  } else {
    esperanza = FinalizationState::GetState();
  }

  esperanza::Result res = esperanza->ValidateVote(
      CScript::ExtractVoteFromSignature(tx.vin[0].scriptSig));

  if (res != +esperanza::Result::SUCCESS) {
    return state.DoS(10, false, REJECT_INVALID, "bad-vote-invalid-esperanza");
  }

  // We keep the check for the prev at the end because is the most expensive
  // check (potentially goes to disk) and there is a good chance that if the
  // vote is not valid (i.e. outdated) then the function will return before
  // reaching this point.
  CTransactionRef prevTx;
  uint256 blockHash;
  // We have to look into the tx database to find the prev deposit, hence the
  // use of fAllowSlow = true
  if (!GetTransaction(tx.vin[0].prevout.hash, prevTx, ::Params().GetConsensus(),
                      blockHash, true)) {
    return state.DoS(10, false, REJECT_INVALID,
                     "bad-vote-no-prev-deposit-found");
  }

  if (!prevTx->IsDeposit() && !prevTx->IsVote()) {
    return state.DoS(10, false, REJECT_INVALID,
                     "bad-vote-prev-not-deposit-or-deposit");
  }

  if (prevTx->vout[0].scriptPubKey != tx.vout[0].scriptPubKey) {
    return state.DoS(10, false, REJECT_INVALID,
                     "bad-vote-not-same-payvoteslash-script");
  }

  return true;
}

}  // namespace esperanza
