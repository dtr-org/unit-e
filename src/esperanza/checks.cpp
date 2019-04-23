// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>
#include <esperanza/adminparams.h>
#include <esperanza/checks.h>
#include <esperanza/finalizationstate.h>
#include <script/interpreter.h>
#include <script/sign.h>
#include <script/standard.h>
#include <txmempool.h>
#include <util.h>
#include <validation.h>

namespace esperanza {

bool ContextualCheckFinalizerCommit(const CTransaction &tx,
                                    CValidationState &err_state,
                                    const FinalizationState &fin_state,
                                    const CCoinsView &view) {
  switch (tx.GetType()) {
    case +TxType::REGULAR:
    case +TxType::COINBASE:
      assert(not("Shouldn't be called on non-finalization transaction"));
    case +TxType::DEPOSIT:
      return ContextualCheckDepositTx(tx, err_state, fin_state);
    case +TxType::VOTE:
      return ContextualCheckVoteTx(tx, err_state, fin_state, view);
    case +TxType::LOGOUT:
      return ContextualCheckLogoutTx(tx, err_state, fin_state, view);
    case +TxType::SLASH:
      return ContextualCheckSlashTx(tx, err_state, fin_state);
    case +TxType::WITHDRAW:
      return ContextualCheckWithdrawTx(tx, err_state, fin_state, view);
    case +TxType::ADMIN:
      return ContextualCheckAdminTx(tx, err_state, fin_state);
  }
  return false;
}

bool CheckFinalizerCommit(const CTransaction &tx, CValidationState &err_state) {
  switch (tx.GetType()) {
    case +TxType::REGULAR:
    case +TxType::COINBASE:
      assert(not("Shouldn't be called on non-finalization transaction"));
    case +TxType::DEPOSIT:
      return CheckDepositTx(tx, err_state, nullptr);
    case +TxType::VOTE:
      return CheckVoteTx(tx, err_state, nullptr, nullptr);
    case +TxType::LOGOUT:
      return CheckLogoutTx(tx, err_state, nullptr);
    case +TxType::SLASH:
      return CheckSlashTx(tx, err_state, nullptr, nullptr);
    case +TxType::WITHDRAW:
      return CheckWithdrawTx(tx, err_state, nullptr);
    case +TxType::ADMIN:
      return CheckAdminTx(tx, err_state, nullptr);
  }
  return false;
}

namespace {
inline bool CheckValidatorAddress(const CTransaction &tx, uint160 *addr_out) {
  static thread_local uint160 addr_tmp;
  if (addr_out == nullptr) {
    addr_out = &addr_tmp;
  }
  return ExtractValidatorAddress(tx, *addr_out);
}

bool FindPrevOutData(const COutPoint &prevout,
                     const CCoinsView &view,
                     TxType *tx_type_out,
                     CScript *script_out) {
  {
    LOCK(mempool.cs);
    CTransactionRef prev_tx = mempool.get(prevout.hash);
    if (prev_tx != nullptr) {
      if (tx_type_out != nullptr) {
        *tx_type_out = prev_tx->GetType();
      }
      if (script_out != nullptr) {
        *script_out = prev_tx->vout[prevout.n].scriptPubKey;
      }
      return true;
    }
  }
  Coin prev_coin;
  if (view.GetCoin(prevout, prev_coin)) {
    if (tx_type_out != nullptr) {
      *tx_type_out = prev_coin.tx_type;
    }
    if (script_out != nullptr) {
      *script_out = prev_coin.out.scriptPubKey;
    }
    return true;
  }
  return false;
}

}  // namespace

bool CheckDepositTx(const CTransaction &tx, CValidationState &err_state,
                    uint160 *validator_address_out) {

  assert(tx.IsDeposit());

  if (tx.vin.empty() || tx.vout.empty()) {
    return err_state.DoS(100, false, REJECT_INVALID, "bad-deposit-malformed");
  }

  if (!tx.vout[0].scriptPubKey.IsFinalizerCommitScript()) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-deposit-vout-script");
  }

  std::vector<std::vector<unsigned char>> solutions;
  txnouttype type_ret;
  if (!Solver(tx.vout[0].scriptPubKey, type_ret, solutions)) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-deposit-script-not-solvable");
  }

  if (!CheckValidatorAddress(tx, validator_address_out)) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-deposit-cannot-extract-validator-address");
  }

  return true;
}

bool ContextualCheckDepositTx(const CTransaction &tx, CValidationState &err_state,
                              const FinalizationState &fin_state) {

  uint160 validator_address;
  if (!CheckDepositTx(tx, err_state, &validator_address)) {
    return false;
  }

  const Result res = fin_state.ValidateDeposit(validator_address, tx.vout[0].nValue);
  switch (+res) {
    case Result::SUCCESS:
      return true;
    case Result::DEPOSIT_DUPLICATE:
      return err_state.Invalid(false, REJECT_INVALID, "bad-deposit-duplicate");
    default:
      return err_state.DoS(100, false, REJECT_INVALID, "bad-deposit-invalid");
  }
}

bool IsVoteExpired(const CTransaction &tx, const FinalizationState &fin_state) {

  assert(tx.IsVote());

  Vote vote;
  std::vector<unsigned char> vote_sig;
  assert(CScript::ExtractVoteFromVoteSignature(tx.vin[0].scriptSig, vote, vote_sig));

  return vote.m_target_epoch < fin_state.GetCurrentEpoch() - 1;
}

bool CheckLogoutTx(const CTransaction &tx, CValidationState &err_state,
                   uint160 *validator_address_out) {

  assert(tx.IsLogout());

  if (tx.vin.size() != 1 || tx.vout.size() != 1) {
    return err_state.DoS(100, false, REJECT_INVALID, "bad-logout-malformed");
  }

  if (!tx.vout[0].scriptPubKey.IsFinalizerCommitScript()) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-logout-vout-script");
  }

  std::vector<std::vector<unsigned char>> solutions;
  txnouttype type_ret;
  if (!Solver(tx.vout[0].scriptPubKey, type_ret, solutions)) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-logout-script-not-solvable");
  }

  if (!CheckValidatorAddress(tx, validator_address_out)) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-logout-cannot-extract-validator-address");
  }

  return true;
}

bool ContextualCheckLogoutTx(const CTransaction &tx, CValidationState &err_state,
                             const FinalizationState &fin_state,
                             const CCoinsView &view) {

  uint160 validator_address = uint160();
  if (!CheckLogoutTx(tx, err_state, &validator_address)) {
    return false;
  }

  const Result res = fin_state.ValidateLogout(validator_address);
  switch (+res) {
    case Result::SUCCESS:
      break;
    case Result::LOGOUT_NOT_A_VALIDATOR:
      return err_state.DoS(100, false, REJECT_INVALID, "bad-logout-not-from-validator");
    default:
      return err_state.DoS(0, false, REJECT_INVALID, "bad-logout-invalid");
  }

  // We keep the check for the prev at the end because is the most expensive
  // check and there is a good chance that if the vote is not valid (i.e. outdated)
  // then the function will return before reaching this point.

  TxType prev_tx_type = TxType::REGULAR;
  CScript prev_out_script;

  if (!FindPrevOutData(tx.vin[0].prevout, view, &prev_tx_type, &prev_out_script)) {
    return err_state.DoS(0, false, REJECT_INVALID, "bad-logout-no-prev-tx-found");
  }

  if (prev_tx_type != +TxType::DEPOSIT && prev_tx_type != +TxType::VOTE) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-logout-prev-not-deposit-or-vote");
  }

  if (prev_out_script != tx.vout[0].scriptPubKey) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-logout-not-same-finalizercommit-script");
  }

  return true;
}

bool CheckWithdrawTx(const CTransaction &tx, CValidationState &err_state,
                     uint160 *out_validator_address) {

  assert(tx.IsWithdraw());

  if (tx.vin.size() != 1 || tx.vout.size() > 3) {
    return err_state.DoS(100, false, REJECT_INVALID, "bad-withdraw-malformed");
  }

  if (!tx.vout[0].scriptPubKey.IsPayToPublicKeyHash()) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-withdraw-vout-script-invalid-p2pkh");
  }

  std::vector<std::vector<unsigned char>> solutions;
  txnouttype type_ret;
  if (!Solver(tx.vout[0].scriptPubKey, type_ret, solutions)) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-withdraw-script-not-solvable");
  }

  if (!CheckValidatorAddress(tx, out_validator_address)) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-withdraw-cannot-extract-validator-address");
  }

  return true;
}

bool ContextualCheckWithdrawTx(const CTransaction &tx, CValidationState &err_state,
                               const FinalizationState &fin_state,
                               const CCoinsView &view) {

  uint160 validator_address = uint160();
  if (!CheckWithdrawTx(tx, err_state, &validator_address)) {
    return false;
  }

  TxType prev_tx_type = TxType::REGULAR;
  CScript prev_out_script;

  if (!FindPrevOutData(tx.vin[0].prevout, view, &prev_tx_type, &prev_out_script)) {
    return err_state.DoS(0, false, REJECT_INVALID,
                         "bad-withdraw-no-prev-tx-found");
  }

  if (prev_tx_type != +TxType::LOGOUT && prev_tx_type != +TxType::VOTE) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-withdraw-prev-not-logout-or-vote");
  }

  std::vector<std::vector<unsigned char>> prev_solutions;
  txnouttype prev_type_ret;
  if (!Solver(prev_out_script, prev_type_ret, prev_solutions)) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-withdraw-script-not-solvable");
  }

  const Result res = fin_state.ValidateWithdraw(validator_address, tx.vout[0].nValue);

  if (res != +Result::SUCCESS) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-withdraw-invalid-state");
  }

  return true;
}

bool CheckVoteTx(const CTransaction &tx, CValidationState &err_state,
                 Vote *vote_out, std::vector<unsigned char> *vote_sig_out) {

  assert(tx.IsVote());

  if (tx.vin.size() != 1 || tx.vout.size() != 1) {
    return err_state.DoS(100, false, REJECT_INVALID, "bad-vote-malformed");
  }

  if (!tx.vout[0].scriptPubKey.IsFinalizerCommitScript()) {
    return err_state.DoS(100, false, REJECT_INVALID, "bad-vote-vout-script");
  }

  static thread_local Vote vote_tmp;
  static thread_local std::vector<unsigned char> vote_sig_tmp;
  if (vote_out == nullptr) {
    vote_out = &vote_tmp;
  }
  if (vote_sig_out == nullptr) {
    vote_sig_out = &vote_sig_tmp;
  }

  if (!CScript::ExtractVoteFromVoteSignature(tx.vin[0].scriptSig, *vote_out, *vote_sig_out)) {
    return err_state.DoS(100, false, REJECT_INVALID, "bad-vote-data-format");
  }

  CPubKey pubkey;
  if (!ExtractValidatorPubkey(tx, pubkey)) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-scriptpubkey-pubkey-format");
  }

  if (!CheckVoteSignature(pubkey, *vote_out, *vote_sig_out)) {
    return err_state.DoS(100, false, REJECT_INVALID, "bad-vote-signature");
  }

  return true;
}

bool ContextualCheckVoteTx(const CTransaction &tx, CValidationState &err_state,
                           const FinalizationState &fin_state,
                           const CCoinsView &view) {

  Vote vote;
  std::vector<unsigned char> vote_sig;
  if (!CheckVoteTx(tx, err_state, &vote, &vote_sig)) {
    return false;
  }

  const Result res = fin_state.ValidateVote(vote);
  switch (+res) {
    case Result::SUCCESS:
      break;
    case Result::VOTE_NOT_BY_VALIDATOR:
      return err_state.DoS(100, false, REJECT_INVALID, "bad-vote-not-from-validator");
    default:
      return err_state.DoS(0, false, REJECT_INVALID, "bad-vote-invalid");
  }

  // We keep the check for the prev at the end because is the most expensive
  // check and there is a good chance that if the vote is not valid (i.e. outdated)
  // then the function will return before reaching this point.

  TxType prev_tx_type = TxType::REGULAR;
  CScript prev_out_script;

  if (!FindPrevOutData(tx.vin[0].prevout, view, &prev_tx_type, &prev_out_script)) {
    return err_state.DoS(0, false, REJECT_INVALID,
                         "bad-vote-no-prev-tx-found");
  }

  if (prev_tx_type != +TxType::DEPOSIT && prev_tx_type != +TxType::VOTE && prev_tx_type != +TxType::LOGOUT) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-vote-prev-not-deposit-vote-or-logout");
  }

  if (prev_out_script != tx.vout[0].scriptPubKey) {
    return err_state.DoS(100, false, REJECT_INVALID,
                         "bad-vote-not-same-finalizercommit-script");
  }

  return true;
}

bool CheckSlashTx(const CTransaction &tx, CValidationState &err_state,
                  Vote *vote1_out, Vote *vote2_out) {

  assert(tx.IsSlash());

  if (tx.vin.size() != 1 || tx.vout.size() != 1) {
    return err_state.DoS(100, false, REJECT_INVALID, "bad-slash-malformed");
  }

  static thread_local Vote vote1_tmp;
  static thread_local Vote vote2_tmp;
  if (vote1_out == nullptr) {
    vote1_out = &vote1_tmp;
  }
  if (vote2_out == nullptr) {
    vote2_out = &vote2_tmp;
  }

  std::vector<unsigned char> vote1_sig;
  std::vector<unsigned char> vote2_sig;
  if (!CScript::ExtractVotesFromSlashSignature(tx.vin[0].scriptSig, *vote1_out,
                                               *vote2_out, vote1_sig, vote2_sig)) {
    return err_state.DoS(100, false, REJECT_INVALID, "bad-slash-data-format");
  }

  return true;
}

bool ContextualCheckSlashTx(const CTransaction &tx, CValidationState &err_state,
                            const FinalizationState &fin_state) {

  Vote vote1;
  Vote vote2;

  if (!CheckSlashTx(tx, err_state, &vote1, &vote2)) {
    return false;
  }

  const esperanza::Result res = fin_state.IsSlashable(vote1, vote2);
  switch (+res) {
    case esperanza::Result::SUCCESS:
      return true;
    case esperanza::Result::SLASH_TOO_EARLY:
    case esperanza::Result::SLASH_ALREADY_SLASHED:
      return err_state.DoS(0, false, REJECT_INVALID, "bad-slash-not-slashable");
    default:
      return err_state.DoS(100, false, REJECT_INVALID, "bad-slash-not-slashable");
  }
}

bool CheckAdminTx(const CTransaction &tx, CValidationState &err_state,
                  std::vector<CPubKey> *keys_out) {

  assert(tx.IsAdmin());

  if (tx.vin.empty()) {
    return err_state.DoS(10, false, REJECT_INVALID, "admin-vin-empty");
  }

  if (tx.vout.empty()) {
    return err_state.DoS(10, false, REJECT_INVALID, "admin-vout-empty");
  }

  size_t valid_commands = 0;
  bool disable_permissioning = false;

  for (const auto &out : tx.vout) {
    AdminCommand command;
    if (!MatchAdminCommand(out.scriptPubKey)) {
      continue;
    }

    if (!DecodeAdminCommand(out.scriptPubKey, command)) {
      return err_state.DoS(10, false, REJECT_INVALID, "admin-invalid-command");
    }

    if (disable_permissioning) {
      return err_state.DoS(10, false, REJECT_INVALID, "admin-double-disable");
    }

    if (command.GetCommandType() ==
        +esperanza::AdminCommandType::END_PERMISSIONING) {
      disable_permissioning = true;
    }

    ++valid_commands;
  }

  if (valid_commands == 0) {
    return err_state.DoS(10, false, REJECT_INVALID, "admin-no-commands");
  }

  const auto &witness = tx.vin.front().scriptWitness;

  static thread_local std::vector<CPubKey> keys_tmp;
  if (keys_out == nullptr) {
    keys_out = &keys_tmp;
  }

  // stack is expected to look like:
  // empty
  // signature
  // ...
  // signature
  // <OP_N> <PubKey> ... <PubKey> <OP_M> <OP_CHECKMULTISIG>
  if (witness.stack.size() != ADMIN_MULTISIG_SIGNATURES + 2 ||
      !CScript::ExtractAdminKeysFromWitness(witness, *keys_out) ||
      keys_out->size() != ADMIN_MULTISIG_KEYS) {
    return err_state.DoS(10, false, REJECT_INVALID, "admin-invalid-witness");
  }

  return true;
}

bool ContextualCheckAdminTx(const CTransaction &tx, CValidationState &err_state,
                            const FinalizationState &fin_state) {

  if (!fin_state.IsPermissioningActive()) {
    return err_state.DoS(10, false, REJECT_INVALID, "admin-disabled");
  }

  std::vector<CPubKey> keys;
  if (!CheckAdminTx(tx, err_state, &keys)) {
    return false;
  }

  AdminKeySet set;
  std::copy_n(keys.begin(), ADMIN_MULTISIG_KEYS, set.begin());
  const auto result = fin_state.ValidateAdminKeys(set);

  if (result != +Result::SUCCESS) {
    return err_state.DoS(10, false, REJECT_INVALID, "admin-not-authorized");
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
        assert(typeRet == TX_COMMIT);
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
