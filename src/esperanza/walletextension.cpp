#include <random>

// Copyright (c) 2018-2019 The Unit-e developers
// Copyright (c) 2017-2018 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/walletextension.h>

#include <blockchain/blockchain_types.h>
#include <consensus/validation.h>
#include <esperanza/checks.h>
#include <esperanza/finalizationstate.h>
#include <net.h>
#include <policy/policy.h>
#include <primitives/txtype.h>
#include <scheduler.h>
#include <script/standard.h>
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
  if (dependencies.settings->node_is_validator) {
    nIsValidatorEnabled = true;
    validatorState = ValidatorState();
  }
}

template <typename Callable>
void WalletExtension::ForEachStakeableCoin(Callable f) const {
  AssertLockHeld(cs_main);
  AssertLockHeld(m_enclosing_wallet.cs_wallet);  // access to mapWallet

  CCoinsViewCache view(pcoinsTip.get());  // requires cs_main
  for (const auto &it : m_enclosing_wallet.mapWallet) {
    const CWalletTx *const tx = &it.second;
    const uint256 &txId = tx->GetHash();
    const std::vector<::CTxOut> &coins = tx->tx->vout;
    const CBlockIndex *containing_block;
    tx->GetDepthInMainChain(containing_block);  // requires cs_main
    if (!containing_block) {
      // transaction is not included in a block
      continue;
    }

    const bool skip_reward = tx->IsCoinBase() && tx->GetBlocksToRewardMaturity() > 0;
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
    const CWalletTx *const tx = &it.second;

    for (const auto &txout : tx->tx->vout) {
      if (::IsStakedRemotely(m_enclosing_wallet, txout.scriptPubKey)) {
        balance += txout.nValue;
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
                                                     CWalletTx *wtx_out, CReserveKey *key_change_out,
                                                     CAmount *fee_out, std::string *error_out,
                                                     const CCoinControl &coin_control) {
  int change_pos_out = -1;
  CAmount fee = 0;
  CWalletTx wtx;
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
  if (type != TX_PUBKEYHASH) {
    *error_out = "Invalid recipient script: must be P2PKH";
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
    const TransactionSignatureCreator sigcreator(&tx_const, i, out.nValue, SIGHASH_ALL);
    if (!ProduceSignature(m_enclosing_wallet, sigcreator, out.scriptPubKey, sigdata)) {
      return false;
    }
    UpdateTransaction(tx, i, sigdata);
  }
  return true;
}

bool WalletExtension::SetMasterKeyFromSeed(const key::mnemonic::Seed &seed,
                                           bool brand_new,
                                           std::string &error) {
  // Backup existing wallet before invalidating keypool
  BackupWallet();

  CKey key = seed.GetExtKey().key;
  const CPubKey hdSeed = m_enclosing_wallet.InitHDSeed(key);
  if (!m_enclosing_wallet.SetHDSeed(hdSeed)) {
    error = "setting master key failed";
    return false;
  }
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
  const std::string wallet_file_name = m_enclosing_wallet.GetName();
  const int64_t current_time = GetTime();

  const std::string backup_wallet_filename =
      wallet_file_name + "~" + std::to_string(current_time);
  const fs::path backup_path = GetDataDir() / backup_wallet_filename;

  return m_enclosing_wallet.BackupWallet(backup_path.string());
}

void WalletExtension::ReadValidatorStateFromFile() {
  if (m_dependencies.settings->node_is_validator && !m_dependencies.settings->node_is_proposer) {
    LogPrint(BCLog::FINALIZATION, "%s: -validating is enabled for wallet %s.\n",
             __func__, m_enclosing_wallet.GetName());

    validatorState = ValidatorState();
    CWalletDB(*m_enclosing_wallet.dbw).ReadValidatorState(*validatorState);
    nIsValidatorEnabled = true;
  }
}

void WalletExtension::WriteValidatorStateToFile() {
  assert(validatorState);
  LogPrintf("Save validator state\n");
  CWalletDB(*m_enclosing_wallet.dbw).WriteValidatorState(*validatorState);
}

bool WalletExtension::SendDeposit(const CKeyID &keyID, CAmount amount,
                                  CWalletTx &wtxOut) {

  assert(validatorState);
  ValidatorState &validator = validatorState.get();
  ValidatorStateWatchWriter validator_writer(*this);

  CCoinControl coinControl;
  CAmount nFeeRet;
  std::string sError;
  int nChangePosInOut = 1;
  std::vector<CRecipient> vecSend;

  CReserveKey reservekey(&m_enclosing_wallet);
  CPubKey pubKey;
  m_enclosing_wallet.GetPubKey(keyID, pubKey);

  CRecipient r{CScript::CreatePayVoteSlashScript(pubKey), amount, true};
  vecSend.push_back(r);

  if (!m_enclosing_wallet.CreateTransaction(
          vecSend, wtxOut, reservekey, nFeeRet, nChangePosInOut, sError,
          coinControl, true, TxType::DEPOSIT)) {

    LogPrint(BCLog::FINALIZATION, "%s: Cannot create deposit transaction.\n",
             __func__);
    return false;
  }

  {
    LOCK2(cs_main, m_enclosing_wallet.cs_wallet);
    CValidationState state;
    if (!m_enclosing_wallet.CommitTransaction(wtxOut, reservekey,
                                              g_connman.get(), state)) {
      LogPrint(BCLog::FINALIZATION, "%s: Cannot commit deposit transaction.\n",
               __func__);
      return false;
    }

    if (state.IsInvalid()) {
      LogPrint(BCLog::FINALIZATION,
               "%s: Cannot verify deposit transaction: %s.\n", __func__,
               state.GetRejectReason());
      return false;
    }

    LogPrint(BCLog::FINALIZATION, "%s: Created new deposit transaction %s.\n",
             __func__, wtxOut.GetHash().GetHex());

    if (validator.m_phase == +ValidatorState::Phase::NOT_VALIDATING) {
      LogPrint(BCLog::FINALIZATION,
               "%s: Validator waiting for deposit confirmation.\n", __func__);

      validator.m_phase = ValidatorState::Phase::WAITING_DEPOSIT_CONFIRMATION;
    } else {
      LogPrintf(
          "ERROR: %s - Wrong state for validator state with deposit %s, %s "
          "expected.\n",
          __func__, wtxOut.GetHash().GetHex(), "WAITING_DEPOSIT_CONFIRMATION");
    }
  }

  return true;
}

bool WalletExtension::SendLogout(CWalletTx &wtxNewOut) {

  assert(validatorState);
  const ValidatorState &validator = validatorState.get();

  wtxNewOut.fTimeReceivedIsTxTime = true;
  wtxNewOut.BindWallet(&m_enclosing_wallet);
  wtxNewOut.fFromMe = true;

  CReserveKey reservekey(&m_enclosing_wallet);
  CValidationState state;

  CMutableTransaction txNew;
  txNew.SetType(TxType::LOGOUT);

  if (validator.m_phase != +ValidatorState::Phase::IS_VALIDATING) {
    return error("%s: Cannot create logouts for non-validators.", __func__);
  }

  CTransactionRef prevTx = validator.m_last_esperanza_tx;

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

  const CAmount fees = GetMinimumFee(nBytes, coinControl, ::mempool, ::feeEstimator, &feeCalc);

  txNew.vout[0].nValue -= fees;

  CTransaction txNewConst(txNew);
  uint32_t nIn = 0;
  SignatureData sigdata;
  std::string strFailReason;

  if (!ProduceSignature(
          m_enclosing_wallet,
          TransactionSignatureCreator(&txNewConst, nIn, amount, SIGHASH_ALL),
          prevScriptPubkey, sigdata, &txNewConst)) {
    return false;
  }
  UpdateTransaction(txNew, nIn, sigdata);

  wtxNewOut.SetTx(MakeTransactionRef(std::move(txNew)));

  {
    LOCK2(cs_main, m_enclosing_wallet.cs_wallet);
    m_enclosing_wallet.CommitTransaction(wtxNewOut, reservekey, g_connman.get(),
                                         state);
    if (state.IsInvalid()) {
      LogPrint(BCLog::FINALIZATION,
               "%s: Cannot commit logout transaction: %s.\n", __func__,
               state.GetRejectReason());
      return false;
    }
  }

  return true;
}

bool WalletExtension::SendWithdraw(const CTxDestination &address,
                                   CWalletTx &wtxNewOut) {

  assert(validatorState);
  const ValidatorState &validator = validatorState.get();

  CCoinControl coinControl;
  coinControl.m_fee_mode = FeeEstimateMode::CONSERVATIVE;

  wtxNewOut.fTimeReceivedIsTxTime = true;
  wtxNewOut.BindWallet(&m_enclosing_wallet);
  wtxNewOut.fFromMe = true;

  CReserveKey reservekey(&m_enclosing_wallet);
  CValidationState errState;
  CKeyID keyID = boost::get<CKeyID>(address);
  CPubKey pubKey;
  m_enclosing_wallet.GetPubKey(keyID, pubKey);

  CMutableTransaction txNew;
  txNew.SetType(TxType::WITHDRAW);

  if (validator.m_phase == +ValidatorState::Phase::IS_VALIDATING) {
    return error("%s: Cannot withdraw with an active validator, logout first.",
                 __func__);
  }

  const std::vector<unsigned char> pkv = ToByteVector(pubKey.GetID());
  const CScript &scriptPubKey = CScript::CreateP2PKHScript(pkv);

  CTransactionRef prevTx = validator.m_last_esperanza_tx;

  const CScript &prevScriptPubkey = prevTx->vout[0].scriptPubKey;

  txNew.vin.push_back(CTxIn(prevTx->GetHash(), 0, CScript(), CTxIn::SEQUENCE_FINAL));

  // Calculate how much we have left of the initial withdraw
  const CAmount initialDeposit = prevTx->vout[0].nValue;
  esperanza::FinalizationState *state = esperanza::FinalizationState::GetState();

  CAmount currentDeposit = 0;

  const esperanza::Result res = state->CalculateWithdrawAmount(
      validator.m_validator_address, currentDeposit);

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

  const CAmount fees = GetMinimumFee(nBytes, coinControl, ::mempool, ::feeEstimator, &feeCalc);

  txNew.vout[0].nValue -= fees;

  CTransaction txNewConst(txNew);
  const uint32_t nIn = 0;
  SignatureData sigdata;

  if (!ProduceSignature(m_enclosing_wallet,
                        TransactionSignatureCreator(
                            &txNewConst, nIn, initialDeposit, SIGHASH_ALL),
                        prevScriptPubkey, sigdata, &txNewConst)) {
    return false;
  }
  UpdateTransaction(txNew, nIn, sigdata);

  wtxNewOut.SetTx(MakeTransactionRef(std::move(txNew)));

  m_enclosing_wallet.CommitTransaction(wtxNewOut, reservekey, g_connman.get(),
                                       errState);
  if (errState.IsInvalid()) {
    LogPrint(BCLog::FINALIZATION,
             "%s: Cannot commit withdraw transaction: %s.\n", __func__,
             errState.GetRejectReason());
    return false;
  }

  return true;
}

void WalletExtension::VoteIfNeeded(const FinalizationState &state) {

  assert(validatorState);
  ValidatorState &validator = validatorState.get();
  ValidatorStateWatchWriter validator_writer(*this);

  const uint32_t dynasty = state.GetCurrentDynasty();

  if (dynasty >= validator.m_end_dynasty) {
    return;
  }

  if (dynasty < validator.m_start_dynasty) {
    return;
  }

  const uint32_t target_epoch = state.GetRecommendedTargetEpoch();
  if (state.GetCurrentEpoch() != target_epoch + 1) {
    // not the right time to vote
    return;
  }

  // Avoid double votes
  if (validator.m_vote_map.find(target_epoch) != validator.m_vote_map.end()) {
    return;
  }

  LogPrint(BCLog::FINALIZATION,
           "%s: Validator voting for epoch %d and dynasty %d.\n", __func__,
           target_epoch, dynasty);

  Vote vote = state.GetRecommendedVote(validator.m_validator_address);
  assert(vote.m_target_epoch == target_epoch);

  // Check for surrounding votes
  if (vote.m_target_epoch < validator.m_last_target_epoch ||
      vote.m_source_epoch < validator.m_last_source_epoch) {

    LogPrint(BCLog::FINALIZATION,
             "%s: Attempting to make a surrounded vote, source: %s, target: %s"
             " prevSource %s, prevTarget: %s.\n",
             __func__, vote.m_source_epoch, vote.m_target_epoch,
             validator.m_last_source_epoch, validator.m_last_target_epoch);
    return;
  }

  CWalletTx createdTx;
  CTransactionRef &prevRef = validator.m_last_esperanza_tx;

  if (SendVote(prevRef, vote, createdTx)) {
    validator.m_vote_map[target_epoch] = vote;
    validator.m_last_target_epoch = vote.m_target_epoch;
    validator.m_last_source_epoch = vote.m_source_epoch;

    LogPrint(BCLog::FINALIZATION, "%s: Casted vote with id %s.\n", __func__,
             createdTx.tx->GetHash().GetHex());
  }
}

bool WalletExtension::SendVote(const CTransactionRef &prevTxRef,
                               const Vote &vote, CWalletTx &wtxNewOut) {

  assert(validatorState);

  wtxNewOut.fTimeReceivedIsTxTime = true;
  wtxNewOut.BindWallet(&m_enclosing_wallet);
  wtxNewOut.fFromMe = true;
  CReserveKey reservekey(&m_enclosing_wallet);
  CValidationState state;

  CMutableTransaction txNew;
  txNew.SetType(TxType::VOTE);

  if (validatorState.get().m_phase != +ValidatorState::Phase::IS_VALIDATING) {
    return error("%s: Cannot create votes for non-validators.", __func__);
  }

  const CScript &scriptPubKey = prevTxRef->vout[0].scriptPubKey;
  const CAmount amount = prevTxRef->vout[0].nValue;

  std::vector<unsigned char> voteSig;
  if (!esperanza::Vote::CreateSignature(&m_enclosing_wallet, vote, voteSig)) {
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
          TransactionSignatureCreator(&txNewConst, nIn, amount, SIGHASH_ALL),
          scriptPubKey, sigdata, &txNewConst)) {
    return error("%s: Cannot produce signature for vote transaction.", __func__);
  }
  UpdateTransaction(txNew, nIn, sigdata);

  wtxNewOut.SetTx(MakeTransactionRef(std::move(txNew)));

  m_enclosing_wallet.CommitTransaction(wtxNewOut, reservekey, g_connman.get(),
                                       state);
  if (state.IsInvalid()) {
    LogPrint(BCLog::FINALIZATION, "%s: Cannot commit vote transaction: %s.\n",
             __func__, state.GetRejectReason());
    return false;
  }

  return true;
}

bool WalletExtension::SendSlash(const finalization::VoteRecord &vote1,
                                const finalization::VoteRecord &vote2) {

  CWalletTx slashTx;
  slashTx.fTimeReceivedIsTxTime = true;
  slashTx.BindWallet(&m_enclosing_wallet);
  slashTx.fFromMe = true;

  CMutableTransaction txNew;
  txNew.SetType(TxType::SLASH);

  CValidationState errState;

  CScript scriptSig = CScript();
  const CScript vote1Script = vote1.GetScript();
  const CScript vote2Script = vote2.GetScript();
  scriptSig << std::vector<unsigned char>(vote1Script.begin(), vote1Script.end());
  scriptSig << std::vector<unsigned char>(vote2Script.begin(), vote2Script.end());
  const CScript burnScript = CScript::CreateUnspendableScript();

  FinalizationState *state = FinalizationState::GetState();
  uint160 validatorAddress = vote1.vote.m_validator_address;
  const uint256 txHash = state->GetLastTxHash(validatorAddress);

  CTransactionRef lastSlashableTx;
  uint256 blockHash;
  GetTransaction(txHash, lastSlashableTx, Params().GetConsensus(), blockHash,
                 true);

  if (!lastSlashableTx) {
    LogPrint(BCLog::FINALIZATION, "%s: Error: previous validator transaction not found: %s.\n",
             __func__, validatorAddress.GetHex());
    return false;
  }

  txNew.vin.push_back(CTxIn(txHash, 0, scriptSig, CTxIn::SEQUENCE_FINAL));

  const CTxOut burnOut(lastSlashableTx->vout[0].nValue, burnScript);
  txNew.vout.push_back(burnOut);

  CTransaction txNewConst(txNew);
  const uint32_t nIn = 0;
  SignatureData sigdata;

  CReserveKey reservekey(&m_enclosing_wallet);
  CPubKey pubKey;
  bool ret;

  ret = reservekey.GetReservedKey(pubKey, true);

  if (!ret) {
    if (!m_enclosing_wallet.GenerateNewKeys(100)) {
      LogPrint(BCLog::FINALIZATION, "%s: Error: No keys available for creating the slashing transaction for: %s.\n",
               __func__, validatorAddress.GetHex());
      return false;
    }

    ret = reservekey.GetReservedKey(pubKey, true);
    if (!ret) {
      LogPrint(BCLog::FINALIZATION, "%s: Error: Cannot reserve pubkey even after top-up for slashing validator: %s.\n",
               __func__, validatorAddress.GetHex());
      return false;
    }
  }

  auto sigCreator = TransactionSignatureCreator(&txNewConst, nIn,
                                                burnOut.nValue, SIGHASH_ALL);

  std::vector<unsigned char> vchSig;
  sigCreator.CreateSig(m_enclosing_wallet, vchSig, pubKey.GetID(), burnOut.scriptPubKey, SigVersion::BASE);
  sigdata.scriptSig = CScript() << vchSig;
  sigdata.scriptSig += scriptSig;

  UpdateTransaction(txNew, nIn, sigdata);

  slashTx.SetTx(MakeTransactionRef(std::move(txNew)));

  m_enclosing_wallet.CommitTransaction(slashTx, reservekey, g_connman.get(),
                                       errState);

  if (errState.IsInvalid()) {
    LogPrint(BCLog::FINALIZATION, "%s: Cannot commit slash transaction: %s.\n",
             __func__, errState.GetRejectReason());

    // We want to relay this transaction in any case, even if for some reason we
    // cannot add it to our mempool
    {
      LOCK(cs_main);
      slashTx.RelayWalletTransaction(g_connman.get());
    }
    return false;
  }

  return true;
}

void WalletExtension::BlockConnected(
    const std::shared_ptr<const CBlock> &pblock, const CBlockIndex &index) {

  LOCK2(cs_main, m_enclosing_wallet.cs_wallet);
  if (nIsValidatorEnabled && !IsInitialBlockDownload()) {

    assert(validatorState);
    ValidatorStateWatchWriter validator_writer(*this);

    switch (validatorState.get().m_phase) {
      case ValidatorState::Phase::IS_VALIDATING: {
        // In case we are logged out, stop validating.
        FinalizationState *state = FinalizationState::GetState();
        assert(state);

        uint32_t currentDynasty = state->GetCurrentDynasty();
        if (currentDynasty >= validatorState.get().m_end_dynasty) {
          LogPrint(BCLog::FINALIZATION, "Validator is disabled because end_dynast=%d reached\n", validatorState.get().m_end_dynasty);
          validatorState.get().m_phase = ValidatorState::Phase::NOT_VALIDATING;
        } else {
          VoteIfNeeded(*state);
        }

        break;
      }
      case ValidatorState::Phase::WAITING_DEPOSIT_FINALIZATION: {
        FinalizationState *state = FinalizationState::GetState();
        assert(state);

        // TODO: UNIT-E: remove "state->GetCurrentEpoch() > 2" when we delete instant finalization
        // and start epoch from 1 #570, #572
        if (state->GetCurrentEpoch() > 2 &&
            state->GetLastFinalizedEpoch() >= validatorState.get().m_deposit_epoch) {
          // Deposit is finalized there is no possible rollback
          validatorState.get().m_phase = ValidatorState::Phase::IS_VALIDATING;

          const esperanza::Validator *validator =
              state->GetValidator(validatorState.get().m_validator_address);

          validatorState.get().m_start_dynasty = validator->m_start_dynasty;

          LogPrint(BCLog::FINALIZATION,
                   "%s: Validator's deposit finalized, the validator index "
                   "is %s.\n",
                   __func__, validatorState.get().m_validator_address.GetHex());
        }
        break;
      }
      default: {
        break;
      }
    }
  }
}

bool WalletExtension::AddToWalletIfInvolvingMe(const CTransactionRef &ptx,
                                               const CBlockIndex *pIndex) {

  const CTransaction &tx = *ptx;

  if (pIndex == nullptr) {
    return true;
  }

  ValidatorStateWatchWriter validator_writer(*this);

  switch (tx.GetType()) {

    case TxType::DEPOSIT: {
      LOCK(m_enclosing_wallet.cs_wallet);
      assert(validatorState);
      esperanza::ValidatorState &state = validatorState.get();

      const auto expectedPhase =
          +esperanza::ValidatorState::Phase::WAITING_DEPOSIT_CONFIRMATION;

      if (state.m_phase == expectedPhase) {

        state.m_phase = esperanza::ValidatorState::Phase::WAITING_DEPOSIT_FINALIZATION;
        LogPrint(BCLog::FINALIZATION,
                 "%s: Validator waiting for deposit finalization. "
                 "Deposit hash %s.\n",
                 __func__, tx.GetHash().GetHex());

        uint160 validatorAddress = uint160();

        if (!esperanza::ExtractValidatorAddress(tx, validatorAddress)) {
          LogPrint(BCLog::FINALIZATION,
                   "ERROR: %s - Cannot extract validator index.\n");
          return false;
        }

        state.m_validator_address = validatorAddress;
        state.m_last_esperanza_tx = ptx;
        state.m_deposit_epoch = esperanza::GetEpoch(*pIndex);

      } else {
        LogPrint(BCLog::FINALIZATION,
                 "ERROR: %s - Wrong state for validator state with "
                 "deposit %s, %s expected but %s found.\n",
                 __func__, tx.GetHash().GetHex(), expectedPhase._to_string(),
                 state.m_phase._to_string());
        return false;
      }
      break;
    }
    case TxType::LOGOUT: {
      LOCK(m_enclosing_wallet.cs_wallet);
      assert(validatorState);
      esperanza::ValidatorState &state = validatorState.get();

      const auto expectedPhase = +esperanza::ValidatorState::Phase::IS_VALIDATING;

      if (state.m_phase == expectedPhase) {

        auto finalizationState = esperanza::FinalizationState::GetState(pIndex);

        const esperanza::Validator *validator =
            finalizationState->GetValidator(state.m_validator_address);

        state.m_end_dynasty = validator->m_end_dynasty;
        state.m_last_esperanza_tx = ptx;

      } else {
        LogPrint(BCLog::FINALIZATION,
                 "ERROR: %s - Wrong state for validator state when "
                 "logging out. %s expected but %s found.\n",
                 __func__, expectedPhase._to_string(),
                 state.m_phase._to_string());
        return false;
      }
      break;
    }
    case TxType::VOTE: {
      LOCK(m_enclosing_wallet.cs_wallet);
      assert(validatorState);
      esperanza::ValidatorState &state = validatorState.get();

      const auto expectedPhase =
          +esperanza::ValidatorState::Phase::IS_VALIDATING;

      if (state.m_phase == expectedPhase) {
        state.m_last_esperanza_tx = ptx;
      } else {
        LogPrint(BCLog::FINALIZATION,
                 "ERROR: %s - Wrong state for validator state when "
                 "voting. %s expected but %s found.\n",
                 __func__, expectedPhase._to_string(),
                 state.m_phase._to_string());
        return false;
      }
      break;
    }
    default: {
      return true;
    }
  }
  return true;
}

const proposer::State &WalletExtension::GetProposerState() const {
  return m_proposer_state;
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

  LOCK(cs_pendingSlashing);
  pendingSlashings.emplace_back(vote1, vote2);
}

void WalletExtension::ManagePendingSlashings() {

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
