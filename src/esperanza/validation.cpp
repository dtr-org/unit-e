// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/params.h>
#include <esperanza/finalizationstate.h>
#include <esperanza/validation.h>
#include <chainparams.h>
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

  esperanza::FinalizationState* esperanza = nullptr;
  if (pindex != nullptr) {
    esperanza = esperanza::FinalizationState::GetState(*pindex);
  } else {
    esperanza = esperanza::FinalizationState::GetState();
  }

  esperanza::Result res =
      esperanza->ValidateDeposit(tx.GetHash(), tx.GetValueOut());

  if (res != esperanza::Result::SUCCESS) {
    return state.DoS(10, false, REJECT_INVALID,
                     "bad-deposit-invalid-esperanza");
  }

  return true;
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

  CTransactionRef prevTx;
  uint256 blockHash;
  GetTransaction(tx.vin[0].prevout.hash, prevTx, ::Params().GetConsensus(),
                 blockHash, false);

  if (!prevTx->IsDeposit() && !prevTx->IsVote()) {
    return state.DoS(10, false, REJECT_INVALID,
                     "bad-vote-prev-not-deposit-or-deposit");
  }

  if (prevTx->vout[0].scriptPubKey != tx.vout[0].scriptPubKey) {
    return state.DoS(10, false, REJECT_INVALID,
                     "bad-vote-not-same-payvoteslash-script");
  }

  esperanza::FinalizationState* esperanza = nullptr;
  if (pindex != nullptr) {
    esperanza = esperanza::FinalizationState::GetState(*pindex);
  } else {
    esperanza = esperanza::FinalizationState::GetState();
  }

  esperanza::Result res = esperanza->ValidateVote(
      CScript::ExtractVoteFromWitness(tx.vin[0].scriptWitness));

  if (res != esperanza::Result::SUCCESS) {
    return state.DoS(10, false, REJECT_INVALID, "bad-vote-invalid-esperanza");
  }

  return true;
}

}  // namespace esperanza
