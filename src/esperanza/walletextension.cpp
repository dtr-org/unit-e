#include <random>

// Copyright (c) 2018-2019 The Unit-e developers
// Copyright (c) 2017-2018 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/walletextension.h>

#include <blockchain/blockchain_types.h>
#include <chainparams.h>
#include <consensus/validation.h>
#include <esperanza/finalizationstate.h>
#include <esperanza/script.h>
#include <finalization/state_repository.h>
#include <net.h>
#include <policy/policy.h>
#include <primitives/txtype.h>
#include <scheduler.h>
#include <script/standard.h>
#include <staking/active_chain.h>
#include <util.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/fees.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>

#include <cstdint>

namespace esperanza {

CCriticalSection cs_pendingSlashing;

WalletExtension::ValidatorStateWatchWriter::~ValidatorStateWatchWriter() {
  if (m_state != m_initial_state) {
    m_wallet.WriteValidatorStateToFile();
  }
}

WalletExtension::WalletExtension(const WalletExtensionDeps &dependencies,
                                 ::CWallet &enclosing_wallet)
    : m_dependencies(dependencies), m_enclosing_wallet(enclosing_wallet) {
  if (dependencies.GetSettings().node_is_validator) {
    nIsValidatorEnabled = true;
    validatorState = ValidatorState();
  }
}

template <typename Callable>
void WalletExtension::ForEachStakeableCoin(Callable f) const {
  AssertLockHeld(cs_main);
  AssertLockHeld(m_enclosing_wallet.cs_wallet);  // access to mapWallet

  auto locked_chain = m_enclosing_wallet.chain().lock();
  CCoinsViewCache view(pcoinsTip.get());  // requires cs_main
  for (const auto &it : m_enclosing_wallet.mapWallet) {
    const CWalletTx *const tx = &it.second;
    const uint256 &txId = tx->GetHash();
    const std::vector<::CTxOut> &coins = tx->tx->vout;
    const CBlockIndex *containing_block = nullptr;
    const int depth = tx->GetDepthInMainChain(*locked_chain);  // requires cs_main
    if (depth <= 0 || !containing_block) {
      // transaction is not included in a block
      continue;
    }

    blockchain::Height height = static_cast<blockchain::Height>(containing_block->nHeight);
    if (!m_dependencies.GetStakeValidator().IsStakeMature(height)) {
      continue;
    }

    const bool skip_reward = tx->IsCoinBase() && tx->GetBlocksToRewardMaturity(*locked_chain) > 0;
    for (std::size_t out_index = skip_reward ? 1 : 0; out_index < coins.size(); ++out_index) {
      if (m_enclosing_wallet.IsSpent(txId, static_cast<unsigned int>(out_index))) {
        continue;
      }
      if (!view.HaveCoin(COutPoint(txId, static_cast<uint32_t>(out_index)))) {
        continue;
      }
      if (m_enclosing_wallet.IsLockedCoin(txId, out_index)) {
        continue;
      }
      const CTxOut &coin = coins[out_index];
      if (coin.nValue <= 0 || !IsStakeableByMe(m_enclosing_wallet, coin.scriptPubKey)) {
        continue;
      }
      f(tx, std::uint32_t(out_index), containing_block);
    }
  }
}

CCriticalSection &WalletExtension::GetLock() const {
  return m_enclosing_wallet.cs_wallet;
}

CAmount WalletExtension::GetReserveBalance() const {
  return m_reserve_balance;
}

CAmount WalletExtension::GetStakeableBalance() const {
  CAmount total_amount = 0;
  ForEachStakeableCoin([&](const CWalletTx *const tx, const std::uint32_t out_index, const CBlockIndex *containing_block) {
    total_amount += tx->tx->vout[out_index].nValue;
  });
  return total_amount;
}

staking::CoinSet WalletExtension::GetStakeableCoins() const {
  staking::CoinSet coins;
  ForEachStakeableCoin([&](const CWalletTx *const tx, const std::uint32_t out_index, const CBlockIndex *containing_block) {
    COutPoint out_point(tx->tx->GetHash(), out_index);
    coins.emplace(containing_block, out_point, tx->tx->vout[out_index]);
  });
  return coins;
}

CAmount WalletExtension::GetRemoteStakingBalance() const {
  AssertLockHeld(cs_main);
  AssertLockHeld(m_enclosing_wallet.cs_wallet);  // access to mapWallet

  CAmount balance = 0;

  for (const auto &it : m_enclosing_wallet.mapWallet) {
    const CWalletTx &tx = it.second;
    const uint256 &tx_hash = tx.GetHash();

    for (size_t i = 0; i < tx.tx->vout.size(); ++i) {
      const CTxOut &tx_out = tx.tx->vout[i];
      if (m_enclosing_wallet.IsSpent(tx_hash, i)) {
        continue;
      }
      if (::IsStakedRemotely(m_enclosing_wallet, tx_out.scriptPubKey)) {
        balance += tx_out.nValue;
      }
    }
  }

  return balance;
}

proposer::State &WalletExtension::GetProposerState() {
  return m_proposer_state;
}

boost::optional<CKey> WalletExtension::GetKey(const CPubKey &pubkey) const {
  const CKeyID keyid = pubkey.GetID();
  CKey key;
  if (!m_enclosing_wallet.GetKey(keyid, key)) {
    return boost::none;
  }
  return key;
}

bool WalletExtension::CreateRemoteStakingTransaction(const CRecipient &recipient,
                                                     CTransactionRef *wtx_out, CReserveKey *key_change_out,
                                                     CAmount *fee_out, std::string *error_out,
                                                     const CCoinControl &coin_control) {
  int change_pos_out = -1;
  CAmount fee = 0;
  CTransactionRef wtx;
  std::string error;

  if (fee_out == nullptr) {
    fee_out = &fee;
  }
  if (error_out == nullptr) {
    error_out = &error;
  }
  if (wtx_out == nullptr) {
    wtx_out = &wtx;
  }

  // The caller must initialize change output key
  assert(key_change_out != nullptr);

  std::vector<std::vector<uint8_t>> solutions;
  txnouttype type;
  if (!Solver(recipient.scriptPubKey, type, solutions)) {
    *error_out = "Invalid scriptPubKey for recipient";
    return false;
  }
  if (type != TX_PUBKEYHASH && type != TX_WITNESS_V0_KEYHASH) {
    *error_out = "Invalid recipient script: must be P2WPKH or P2PKH";
    return false;
  }
  if (solutions.size() != 1 || solutions[0].size() != 20) {
    *error_out = "Invalid address for recipient: must be a single 160-bit pubkey hash";
    return false;
  }

  CPubKey spending_key;
  m_enclosing_wallet.GetKeyFromPool(spending_key, false);

  CRecipient staking_recipient = recipient;
  staking_recipient.scriptPubKey = CScript::CreateRemoteStakingKeyhashScript(
      solutions[0], ToByteVector(spending_key.GetSha256()));

  const std::vector<CRecipient> recipients = {staking_recipient};

  return m_enclosing_wallet.CreateTransaction(
      recipients, *wtx_out, *key_change_out, *fee_out, change_pos_out, *error_out, coin_control);
}

bool WalletExtension::SignCoinbaseTransaction(CMutableTransaction &tx) {
  AssertLockHeld(GetLock());

  const CTransaction tx_const(tx);
  const auto &wallet = m_enclosing_wallet.mapWallet;
  for (unsigned int i = 1; i < tx.vin.size(); ++i) {  // skips the first input, which is the meta input
    const auto &input = tx.vin[i];
    const auto index = input.prevout.n;
    const auto mi = wallet.find(input.prevout.hash);
    if (mi == wallet.end()) {
      return false;
    }
    const auto &vout = mi->second.tx->vout;
    if (index >= vout.size()) {
      return false;
    }
    const CTxOut &out = vout[index];
    SignatureData sigdata;
    const MutableTransactionSignatureCreator sigcreator(&tx, i, out.nValue, SIGHASH_ALL);
    if (!ProduceSignature(m_enclosing_wallet, sigcreator, out.scriptPubKey, sigdata)) {
      return false;
    }
    UpdateInput(tx.vin.at(i), sigdata);
  }
  return true;
}

const std::string &WalletExtension::GetName() const {
  return m_enclosing_wallet.GetName();
}

bool WalletExtension::SetMasterKeyFromSeed(const key::mnemonic::Seed &seed,
                                           bool brand_new,
                                           std::string &error) {
  // Backup existing wallet before invalidating keypool
  BackupWallet();

  CKey key = seed.GetExtKey().key;
  const CPubKey hdSeed = m_enclosing_wallet.DeriveNewSeed(key);
  m_enclosing_wallet.SetHDSeed(hdSeed);
  // If there's a chance that the derived keys could have been used in the
  // blockchain, set their key time to 1
  RAIIMockTime timekeeper(1, !brand_new);
  if (!m_enclosing_wallet.NewKeyPool()) {
    error = "could not generate new keypool";
    return false;
  }
  return true;
}

bool WalletExtension::BackupWallet() {
  const std::string wallet_file_name = m_enclosing_wallet.GetName().empty() ? "wallet.dat" : m_enclosing_wallet.GetName();
  const int64_t current_time = GetTime();

  const std::string backup_wallet_filename =
      wallet_file_name + "~" + std::to_string(current_time);
  const fs::path backup_path = GetDataDir() / backup_wallet_filename;

  return m_enclosing_wallet.BackupWallet(backup_path.string());
}

void WalletExtension::ReadValidatorStateFromFile() {
  if (m_dependencies.GetSettings().node_is_validator && !m_dependencies.GetSettings().node_is_proposer) {
    LogPrint(BCLog::FINALIZATION, "%s: -validating is enabled for wallet %s.\n",
             __func__, m_enclosing_wallet.GetName());

    validatorState = ValidatorState();
    WalletBatch(m_enclosing_wallet.GetDBHandle()).ReadValidatorState(*validatorState);
    nIsValidatorEnabled = true;
  }
}

void WalletExtension::WriteValidatorStateToFile() {
  assert(validatorState);
  WalletBatch(m_enclosing_wallet.GetDBHandle()).WriteValidatorState(*validatorState);
}

bool WalletExtension::SendDeposit(const CKeyID &keyID, CAmount amount,
                                  CTransactionRef &wtxOut) {

  assert(validatorState);

  LOCK2(cs_main, m_enclosing_wallet.cs_wallet);

  LOCK(m_dependencies.GetFinalizationStateRepository().GetLock());
  const FinalizationState *fin_state = m_dependencies.GetFinalizationStateRepository().GetTipState();
  assert(fin_state);

  const esperanza::Result is_valid = fin_state->ValidateDeposit(keyID, amount);
  if (is_valid != +esperanza::Result::SUCCESS) {
    LogPrintf("Cannot send deposit to %s, check above logs for details\n", keyID.GetHex());
    return false;
  }

  const ValidatorState::Phase cur_phase = GetFinalizerPhase(*fin_state);
  const ValidatorState::Phase exp_phase = ValidatorState::Phase::NOT_VALIDATING;
  if (cur_phase != exp_phase) {
    LogPrint(BCLog::FINALIZATION, /* Continued */
             "ERROR: %s: can't send deposit because finalizer in phase=%s when expected=%s\n",
             __func__,
             cur_phase._to_string(),
             exp_phase._to_string());
    return false;
  }

  ValidatorState &validator = validatorState.get();
  ValidatorStateWatchWriter validator_writer(*this);

  CCoinControl coinControl;
  CAmount nFeeRet;
  std::string sError;
  int nChangePosInOut = 1;
  std::vector<CRecipient> vecSend;

  CReserveKey reservekey(&m_enclosing_wallet);
  CPubKey pubKey;
  if (!m_enclosing_wallet.GetPubKey(keyID, pubKey)) {
    LogPrint(BCLog::FINALIZATION, "%s: Cannot deposit to an unknown address.\n",
             __func__);
    return false;
  }

  CRecipient r{CScript::CreateFinalizerCommitScript(pubKey), amount, /*fSubtractFeeFromAmount=*/false};
  vecSend.push_back(r);

  if (!m_enclosing_wallet.CreateTransaction(
          vecSend, wtxOut, reservekey, nFeeRet, nChangePosInOut, sError,
          coinControl, true, TxType::DEPOSIT)) {

    LogPrint(BCLog::FINALIZATION, "%s: Cannot create deposit transaction. %s\n",
             __func__, sError);
    return false;
  }

  {
    CValidationState state;
    if (!m_enclosing_wallet.CommitTransaction(wtxOut, {}, {}, {}, reservekey,
                                              g_connman.get(), state)) {
      LogPrint(BCLog::FINALIZATION, "%s: Cannot commit deposit transaction.\n",
               __func__);
      return false;
    }

    if (state.IsInvalid()) {
      LogPrint(BCLog::FINALIZATION, /* Continued */
               "%s: Cannot verify deposit transaction: %s.\n", __func__,
               state.GetRejectReason());
      return false;
    }

    validator.m_last_deposit_tx = wtxOut->GetHash();
    LogPrint(BCLog::FINALIZATION, "%s: Created new deposit transaction %s.\n",
             __func__, wtxOut->GetHash().GetHex());
  }

  return true;
}

bool WalletExtension::SendLogout(CTransactionRef &wtxNewOut) {
  assert(validatorState);

  LOCK2(cs_main, m_enclosing_wallet.cs_wallet);

  LOCK(m_dependencies.GetFinalizationStateRepository().GetLock());
  const FinalizationState *state = m_dependencies.GetFinalizationStateRepository().GetTipState();
  assert(state);

  const esperanza::Validator *validator = state->GetValidator(validatorState->m_validator_address);
  if (!validator) {
    return error(
        "%s: this wallet doesn't have associated finalizer "
        "because no deposit from this wallet was made",
        __func__);
  }

  if (!state->IsFinalizerVoting(*validator)) {
    return error("%s: Cannot create logouts for non-validators.", __func__);
  }

  CReserveKey reservekey(&m_enclosing_wallet);

  CMutableTransaction txNew;
  txNew.SetType(TxType::LOGOUT);

  CTransactionRef prevTx;
  {
    const CWalletTx *prev_tx = m_enclosing_wallet.GetWalletTx(validator->m_last_transaction_hash);
    assert(prev_tx);
    prevTx = prev_tx->tx;
  }

  const CScript &prevScriptPubkey = prevTx->vout[0].scriptPubKey;
  CAmount amount = prevTx->vout[0].nValue;

  // We need to pay some minimal fees if we wanna make sure that the logout
  // will be included.
  FeeCalculation feeCalc;

  txNew.vin.push_back(
      CTxIn(prevTx->GetHash(), 0, CScript(), CTxIn::SEQUENCE_FINAL));

  CTxOut txout(amount, prevScriptPubkey);
  txNew.vout.push_back(txout);

  const auto nBytes = static_cast<unsigned int>(GetVirtualTransactionSize(txNew));

  CCoinControl coinControl;
  coinControl.m_fee_mode = FeeEstimateMode::CONSERVATIVE;

  const CAmount fees = GetMinimumFee(m_enclosing_wallet, nBytes, coinControl, ::mempool, ::feeEstimator, &feeCalc);

  txNew.vout[0].nValue -= fees;

  CTransaction txNewConst(txNew);
  uint32_t nIn = 0;
  SignatureData sigdata;
  std::string strFailReason;

  if (!ProduceSignature(
          m_enclosing_wallet,
          MutableTransactionSignatureCreator(&txNew, nIn, amount, SIGHASH_ALL),
          prevScriptPubkey, sigdata, &txNewConst)) {
    return false;
  }
  UpdateInput(txNew.vin.at(nIn), sigdata);

  wtxNewOut = MakeTransactionRef(std::move(txNew));

  CValidationState validation_state;
  m_enclosing_wallet.CommitTransaction(wtxNewOut, {}, {}, {}, reservekey, g_connman.get(),
                                       validation_state);
  if (validation_state.IsInvalid()) {
    LogPrint(BCLog::FINALIZATION, /* Continued */
             "%s: Cannot commit logout transaction: %s.\n", __func__,
             validation_state.GetRejectReason());
    return false;
  }

  // must be forgotten as if we sent logout, deposit can't be reverted
  // and GetFinalizerPhase() must rely on global FinalizationState
  validatorState->m_last_deposit_tx.SetNull();

  return true;
}

bool WalletExtension::SendWithdraw(const CTxDestination &address, CTransactionRef &wtxNewOut) {
  assert(validatorState);

  LOCK2(cs_main, m_enclosing_wallet.cs_wallet);

  LOCK(m_dependencies.GetFinalizationStateRepository().GetLock());
  const FinalizationState *state = m_dependencies.GetFinalizationStateRepository().GetTipState();
  assert(state);

  const esperanza::Validator *validator = state->GetValidator(validatorState->m_validator_address);
  if (!validator) {
    return error(
        "%s: this wallet doesn't have associated finalizer "
        "because no deposit from this wallet was made",
        __func__);
  }

  const ValidatorState::Phase cur_phase = GetFinalizerPhase(*state);
  const ValidatorState::Phase exp_phase = ValidatorState::Phase::WAITING_TO_WITHDRAW;
  if (cur_phase != exp_phase) {
    LogPrint(BCLog::FINALIZATION, /* Continued */
             "ERROR: %s: can't send withdraw because finalizer in phase=%s when expected=%s\n",
             __func__,
             cur_phase._to_string(),
             exp_phase._to_string());
    return false;
  }

  CCoinControl coinControl;
  coinControl.m_fee_mode = FeeEstimateMode::CONSERVATIVE;

  CReserveKey reservekey(&m_enclosing_wallet);
  CValidationState errState;
  CKeyID keyID = boost::get<CKeyID>(address);
  CPubKey pubKey;
  m_enclosing_wallet.GetPubKey(keyID, pubKey);

  CMutableTransaction txNew;
  txNew.SetType(TxType::WITHDRAW);

  const std::vector<unsigned char> pkv = ToByteVector(pubKey.GetID());
  const CScript &scriptPubKey = CScript::CreateP2PKHScript(pkv);

  CTransactionRef prevTx;
  {
    const CWalletTx *prev_tx = m_enclosing_wallet.GetWalletTx(validator->m_last_transaction_hash);
    assert(prev_tx);
    prevTx = prev_tx->tx;
  }

  const CScript &prevScriptPubkey = prevTx->vout[0].scriptPubKey;

  txNew.vin.push_back(CTxIn(prevTx->GetHash(), 0, CScript(), CTxIn::SEQUENCE_FINAL));

  // Calculate how much we have left of the initial withdraw
  const CAmount initialDeposit = prevTx->vout[0].nValue;

  CAmount currentDeposit = 0;

  const esperanza::Result res = state->CalculateWithdrawAmount(
      validatorState->m_validator_address, currentDeposit);

  if (res != +Result::SUCCESS) {
    LogPrint(BCLog::FINALIZATION, "%s: Cannot calculate withdraw amount: %s.\n",
             __func__, res._to_string());
    return false;
  }

  const CAmount toWithdraw = std::min(currentDeposit, initialDeposit);

  const CTxOut txout(toWithdraw, scriptPubKey);
  txNew.vout.push_back(txout);

  const CAmount amountToBurn = initialDeposit - toWithdraw;

  if (amountToBurn > 0) {
    const CTxOut burnTx(amountToBurn, CScript::CreateUnspendableScript());
    txNew.vout.push_back(burnTx);
  }

  // We need to pay some minimal fees if we wanna make sure that the withdraw
  // will be included.
  FeeCalculation feeCalc;

  const auto nBytes = static_cast<unsigned int>(GetVirtualTransactionSize(txNew));

  const CAmount fees = GetMinimumFee(m_enclosing_wallet, nBytes, coinControl, ::mempool, ::feeEstimator, &feeCalc);

  txNew.vout[0].nValue -= fees;

  CTransaction txNewConst(txNew);
  const uint32_t nIn = 0;
  SignatureData sigdata;

  if (!ProduceSignature(m_enclosing_wallet,
                        MutableTransactionSignatureCreator(
                            &txNew, nIn, initialDeposit, SIGHASH_ALL),
                        prevScriptPubkey, sigdata, &txNewConst)) {
    return false;
  }
  UpdateInput(txNew.vin.at(nIn), sigdata);

  wtxNewOut = MakeTransactionRef(std::move(txNew));

  m_enclosing_wallet.CommitTransaction(wtxNewOut, {}, {}, {}, reservekey, g_connman.get(),
                                       errState);
  if (errState.IsInvalid()) {
    LogPrint(BCLog::FINALIZATION, /* Continued */
             "%s: Cannot commit withdraw transaction: %s.\n", __func__,
             errState.GetRejectReason());
    return false;
  }

  return true;
}

void WalletExtension::VoteIfNeeded() {
  LOCK2(cs_main, m_enclosing_wallet.cs_wallet);

  LOCK(m_dependencies.GetFinalizationStateRepository().GetLock());
  const CBlockIndex *tip_block_index = m_dependencies.GetActiveChain().GetTip();
  const FinalizationState *fin_state = m_dependencies.GetFinalizationStateRepository().Find(*tip_block_index);
  assert(fin_state);

  if (GetFinalizerPhase(*fin_state) != +ValidatorState::Phase::IS_VALIDATING) {
    return;
  }

  assert(validatorState);

  ValidatorStateWatchWriter validator_writer(*this);
  const esperanza::Validator *validator = fin_state->GetValidator(validatorState->m_validator_address);
  assert(validator);

  const uint32_t block_number = tip_block_index->nHeight % fin_state->GetEpochLength();
  if (block_number < m_dependencies.GetSettings().finalizer_vote_from_epoch_block_number) {
    return;
  }

  const uint32_t target_epoch = fin_state->GetRecommendedTargetEpoch();
  if (fin_state->GetCurrentEpoch() != target_epoch + 1) {
    // not the right time to vote
    return;
  }

  // Avoid double votes
  if (validatorState->m_vote_map.find(target_epoch) != validatorState->m_vote_map.end()) {
    return;
  }

  LogPrint(BCLog::FINALIZATION, /* Continued */
           "%s: Validator voting for epoch %d and dynasty %d.\n", __func__,
           target_epoch, fin_state->GetCurrentDynasty());

  Vote vote = fin_state->GetRecommendedVote(validatorState->m_validator_address);
  assert(vote.m_target_epoch == target_epoch);

  // Check for surrounding votes
  if (vote.m_target_epoch < validatorState->m_last_target_epoch ||
      vote.m_source_epoch < validatorState->m_last_source_epoch) {

    LogPrint(BCLog::FINALIZATION,                                               /* Continued */
             "%s: Attempting to make a surrounded vote, source: %s, target: %s" /* Continued */
             " prevSource %s, prevTarget: %s.\n",
             __func__, vote.m_source_epoch, vote.m_target_epoch,
             validatorState->m_last_source_epoch, validatorState->m_last_target_epoch);
    return;
  }

  const CWalletTx *prev_tx = m_enclosing_wallet.GetWalletTx(validator->m_last_transaction_hash);
  assert(prev_tx);

  CTransactionRef createdTx;
  if (SendVote(prev_tx->tx, vote, createdTx)) {

    LogPrint(BCLog::FINALIZATION, "%s: Casted vote with id %s.\n", __func__,
             createdTx->GetHash().GetHex());
  }
}

bool WalletExtension::SendVote(const CTransactionRef &prevTxRef,
                               const Vote &vote, CTransactionRef &wtxNewOut) {

  AssertLockHeld(m_enclosing_wallet.cs_wallet);

  assert(validatorState);

  ValidatorState &validator = validatorState.get();

  CReserveKey reservekey(&m_enclosing_wallet);
  CValidationState state;

  CMutableTransaction txNew;
  txNew.SetType(TxType::VOTE);

  const CScript &scriptPubKey = prevTxRef->vout[0].scriptPubKey;
  const CAmount amount = prevTxRef->vout[0].nValue;

  std::vector<unsigned char> voteSig;
  if (!CreateVoteSignature(&m_enclosing_wallet, vote, voteSig)) {
    return error("%s: Cannot sign vote.", __func__);
  }
  CScript scriptSig = CScript::EncodeVote(vote, voteSig);

  txNew.vin.push_back(
      CTxIn(prevTxRef->GetHash(), 0, scriptSig, CTxIn::SEQUENCE_FINAL));

  CTxOut txout(amount, scriptPubKey);
  txNew.vout.push_back(txout);

  CTransaction txNewConst(txNew);
  uint32_t nIn = 0;
  SignatureData sigdata;

  if (!ProduceSignature(
          m_enclosing_wallet,
          MutableTransactionSignatureCreator(&txNew, nIn, amount, SIGHASH_ALL),
          scriptPubKey, sigdata, &txNewConst)) {
    return error("%s: Cannot produce signature for vote transaction.", __func__);
  }
  UpdateInput(txNew.vin.at(nIn), sigdata);

  wtxNewOut = MakeTransactionRef(std::move(txNew));

  CWalletTx *wtx_new = nullptr;

  CConnman *connman = g_connman.get();

  m_enclosing_wallet.CommitTransaction(wtxNewOut, {}, {}, {}, reservekey, g_connman.get(), state, /*relay*/ false, &wtx_new);
  if (state.IsInvalid()) {
    LogPrint(BCLog::FINALIZATION, "%s: Cannot commit vote transaction: %s.\n",
             __func__, state.GetRejectReason());
    return false;
  }

  validator.m_vote_map[vote.m_target_epoch] = vote;
  validator.m_last_target_epoch = vote.m_target_epoch;
  validator.m_last_source_epoch = vote.m_source_epoch;
  WriteValidatorStateToFile();

  bool embargoed = false;
  if (connman != nullptr && wtx_new->tx->GetType() == +TxType::REGULAR && connman->embargoman) {
    embargoed = connman->embargoman->SendTransactionAndEmbargo(*wtx_new->tx);
  }

  if (!embargoed) {
    wtx_new->RelayWalletTransaction(connman);
  }

  return true;
}

bool WalletExtension::SendSlash(const finalization::VoteRecord &vote1,
                                const finalization::VoteRecord &vote2) {

  CMutableTransaction txNew;
  txNew.SetType(TxType::SLASH);

  CValidationState errState;

  CScript scriptSig = CScript();
  const CScript vote1Script = vote1.GetScript();
  const CScript vote2Script = vote2.GetScript();
  scriptSig << std::vector<unsigned char>(vote1Script.begin(), vote1Script.end());
  scriptSig << std::vector<unsigned char>(vote2Script.begin(), vote2Script.end());
  const CScript burnScript = CScript::CreateUnspendableScript();

  const uint160 validatorAddress = vote1.vote.m_validator_address;
  uint256 txHash;

  {
    LOCK(m_dependencies.GetFinalizationStateRepository().GetLock());
    const FinalizationState *fin_state = m_dependencies.GetFinalizationStateRepository().GetTipState();
    assert(fin_state != nullptr);

    txHash = fin_state->GetLastTxHash(validatorAddress);
  }

  CTransactionRef lastSlashableTx;
  uint256 blockHash;
  GetTransaction(txHash, lastSlashableTx, ::Params().GetConsensus(), blockHash,
                 true);

  if (!lastSlashableTx) {
    LogPrint(BCLog::FINALIZATION, "%s: Error: previous validator transaction not found: %s.\n",
             __func__, validatorAddress.GetHex());
    return false;
  }

  txNew.vin.push_back(CTxIn(txHash, 0, scriptSig, CTxIn::SEQUENCE_FINAL));

  const CTxOut burnOut(lastSlashableTx->vout[0].nValue, burnScript);
  txNew.vout.push_back(burnOut);

  CReserveKey reservekey(&m_enclosing_wallet);

  auto txref = MakeTransactionRef(std::move(txNew));

  m_enclosing_wallet.CommitTransaction(txref, {}, {}, {}, reservekey, g_connman.get(),
                                       errState);

  if (errState.IsInvalid()) {
    LogPrint(BCLog::FINALIZATION, "%s: Cannot commit slash transaction: %s.\n",
             __func__, errState.GetRejectReason());

    // We want to relay this transaction in any case, even if for some reason we
    // cannot add it to our mempool
    {
      LOCK2(cs_main, m_enclosing_wallet.cs_wallet);
      auto it = m_enclosing_wallet.mapWallet.find(txref->GetHash());
      if (it != m_enclosing_wallet.mapWallet.end()) {
        CWalletTx &slash_tx = it->second;
        slash_tx.RelayWalletTransaction(g_connman.get());
      }
    }
    return false;
  }

  return true;
}

void WalletExtension::BlockConnected(
    const std::shared_ptr<const CBlock> &pblock, const CBlockIndex &index) {

  if (!nIsValidatorEnabled) {
    // finalizer is explicitly disabled
    return;
  }

  if (IsInitialBlockDownload()) {
    // there is no reason to vote as such votes will be outdated
    // and won't be included in the chain
    return;
  }

  VoteIfNeeded();
}

bool WalletExtension::AddToWalletIfInvolvingMe(const CTransactionRef &ptx,
                                               const CBlockIndex *pIndex) {
  if (!nIsValidatorEnabled) {
    return true;
  }

  const CTransaction &tx = *ptx;

  if (pIndex == nullptr) {
    return true;
  }

  assert(validatorState);

  LOCK(m_enclosing_wallet.cs_wallet);
  ValidatorStateWatchWriter validator_writer(*this);

  LOCK(m_dependencies.GetFinalizationStateRepository().GetLock());
  const FinalizationState *fin_state = m_dependencies.GetFinalizationStateRepository().GetTipState();
  assert(fin_state != nullptr);

  switch (tx.GetType()) {
    case TxType::DEPOSIT: {
      const ValidatorState::Phase cur_phase = GetFinalizerPhase(*fin_state);
      if (cur_phase != +ValidatorState::Phase::NOT_VALIDATING &&                // deposit received via (re-)indexing
          cur_phase != +ValidatorState::Phase::WAITING_DEPOSIT_CONFIRMATION) {  // deposit was sent explicitly
        LogPrint(BCLog::FINALIZATION,                                           /* Continued */
                 "ERROR: %s: finalizer=%s has already created a deposit.\n",
                 __func__,
                 validatorState->m_validator_address.ToString());
        return false;
      }

      uint160 finalizer_address;
      if (!esperanza::ExtractValidatorAddress(tx, finalizer_address)) {
        LogPrint(BCLog::FINALIZATION, /* Continued */
                 "ERROR: %s: Cannot extract validator address.\n",
                 __func__);
        return false;
      }
      validatorState->m_validator_address = finalizer_address;
      break;
    }
    case TxType::LOGOUT: {
      const esperanza::Validator *validator = fin_state->GetValidator(validatorState->m_validator_address);
      if (!validator) {
        LogPrint(BCLog::FINALIZATION, /* Continued */
                 "ERROR: %s: finalizer=%s can't logout because deposit is missing\n",
                 __func__,
                 validatorState->m_validator_address.ToString());
        return false;
      }

      if (!fin_state->IsFinalizerVoting(*validator)) {
        LogPrint(BCLog::FINALIZATION,                                                     /* Continued */
                 "ERROR: %s: finalizer=%s can't logout because not in the voting state. " /* Continued */
                 "current_dynasty=%d start_dynasty=%d end_dynasty=%d\n",
                 __func__, validatorState->m_validator_address.ToString(),
                 fin_state->GetCurrentDynasty(),
                 validator->m_start_dynasty,
                 validator->m_end_dynasty);
        return false;
      }

      // at this point we don't care about the last deposit as it can't be reverted.
      // we must reset this field not to misinterpret NOT_VALIDATING as WAITING_DEPOSIT_CONFIRMATION
      validatorState->m_last_deposit_tx.SetNull();
      break;
    }
    case TxType::VOTE: {
      const esperanza::Validator *validator = fin_state->GetValidator(validatorState->m_validator_address);
      if (!validator) {
        LogPrint(BCLog::FINALIZATION, /* Continued */
                 "ERROR: %s: finalizer=%s can't vote because deposit is missing\n",
                 __func__,
                 validatorState->m_validator_address.ToString());
        return false;
      }

      if (!fin_state->IsFinalizerVoting(*validator)) {
        LogPrint(BCLog::FINALIZATION,                                                   /* Continued */
                 "ERROR: %s: finalizer=%s can't vote because not in the voting state. " /* Continued */
                 "current_dynasty=%d start_dynasty=%d end_dynasty=%d\n",
                 __func__, validatorState->m_validator_address.ToString(),
                 fin_state->GetCurrentDynasty(),
                 validator->m_start_dynasty,
                 validator->m_end_dynasty);
        return false;
      }
      break;
    }
    case TxType::WITHDRAW: {
      const ValidatorState::Phase cur_phase = GetFinalizerPhase(*fin_state);
      if (cur_phase != +ValidatorState::Phase::WAITING_TO_WITHDRAW &&  // __func__ is called before block is processed
          cur_phase != +ValidatorState::Phase::NOT_VALIDATING) {       // __func__ is called after block is processed
        LogPrint(BCLog::FINALIZATION,                                  /* Continued */
                 "ERROR: %s: finalizer=%s can't withdraw as it's still validating.\n",
                 __func__,
                 validatorState->m_validator_address.ToString());
        return false;
      }

      validatorState = ValidatorState();
      break;
    }
    case TxType::SLASH:
    case TxType::ADMIN:
    case TxType::REGULAR:
    case TxType::COINBASE:
      break;
  }

  return true;
}

const proposer::State &WalletExtension::GetProposerState() const {
  return m_proposer_state;
}

const ValidatorState::Phase WalletExtension::GetFinalizerPhase(const FinalizationState &state) const {
  if (!nIsValidatorEnabled) {
    return ValidatorState::Phase::NOT_VALIDATING;
  }

  if (!validatorState) {
    return ValidatorState::Phase::NOT_VALIDATING;
  }

  // check if finalizer created deposit
  // but it's not included in the chain yet
  const esperanza::Validator *validator = state.GetValidator(validatorState->m_validator_address);
  if (!validator) {
    if (validatorState->m_last_deposit_tx.IsNull()) {
      return ValidatorState::Phase::NOT_VALIDATING;
    }

    const CWalletTx *tx = m_enclosing_wallet.GetWalletTx(validatorState->m_last_deposit_tx);
    if (!tx) {
      return ValidatorState::Phase::NOT_VALIDATING;
    }

    return ValidatorState::Phase::WAITING_DEPOSIT_CONFIRMATION;
  }

  if (state.GetCurrentDynasty() < validator->m_start_dynasty) {
    return ValidatorState::Phase::WAITING_DEPOSIT_FINALIZATION;
  }

  if (state.IsFinalizerVoting(*validator)) {
    return ValidatorState::Phase::IS_VALIDATING;
  }

  if (state.GetCurrentEpoch() < state.CalculateWithdrawEpoch(*validator)) {
    return ValidatorState::Phase::WAITING_FOR_WITHDRAW_DELAY;
  }

  return ValidatorState::Phase::WAITING_TO_WITHDRAW;
}

EncryptionState WalletExtension::GetEncryptionState() const {
  if (!m_enclosing_wallet.IsCrypted()) {
    return EncryptionState::UNENCRYPTED;
  }
  if (m_enclosing_wallet.IsLocked()) {
    return EncryptionState::LOCKED;
  }
  if (m_unlocked_for_staking_only) {
    return EncryptionState::UNLOCKED_FOR_STAKING_ONLY;
  }
  return EncryptionState::UNLOCKED;
}

bool WalletExtension::Unlock(const SecureString &wallet_passphrase,
                             bool for_staking_only) {
  m_unlocked_for_staking_only = for_staking_only;
  return m_enclosing_wallet.Unlock(wallet_passphrase);
}

void WalletExtension::SlashingConditionDetected(
    const finalization::VoteRecord &vote1,
    const finalization::VoteRecord &vote2) {

  if (validatorState) {
    ValidatorState &state = validatorState.get();

    if (vote1.vote.m_validator_address == state.m_validator_address) {
      LogPrint(BCLog::FINALIZATION, /* Continued */
               "WARNING: %s - The finalizer is trying to slash itself. vote1=%s vote2=%s.\n",
               __func__, vote1.vote.ToString(), vote2.vote.ToString());
      return;
    }
  }

  LOCK(cs_pendingSlashing);
  pendingSlashings.emplace_back(vote1, vote2);
}

std::shared_ptr<CWallet> GetWalletHandle(CWallet *pwallet) {
  for (std::shared_ptr<CWallet> w : GetWallets()) {
    if (w.get() == pwallet) {
      return w;
    }
  }
  return nullptr;
}

void WalletExtension::ManagePendingSlashings() {
  // Ensure the wallet won't be freed while we need it
  std::shared_ptr<CWallet> wallet = GetWalletHandle(&m_enclosing_wallet);
  if (!wallet) {
    throw task_unscheduled();
  }

  if (pendingSlashings.empty()) {
    return;
  }

  std::vector<std::pair<finalization::VoteRecord, finalization::VoteRecord>> pendings;

  {
    LOCK(cs_pendingSlashing);
    std::swap(pendings, pendingSlashings);
  }

  for (auto const &pair : pendings) {
    SendSlash(pair.first, pair.second);
  }
}

void WalletExtension::PostInitProcess(CScheduler &scheduler) {

  scheduler.scheduleEvery(std::bind(&WalletExtension::ManagePendingSlashings, this), 10000);
}

}  // namespace esperanza
