// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017-2018 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/walletextension.h>

#include <random>

#include <consensus/validation.h>
#include <esperanza/finalizationstate.h>
#include <net.h>
#include <policy/policy.h>
#include <primitives/txtype.h>
#include <script/sign.h>
#include <script/standard.h>
#include <util.h>
#include <utilfun.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/fees.h>
#include <wallet/wallet.h>

namespace esperanza {

WalletExtension::WalletExtension(const Settings &settings,
                                 ::CWallet *enclosingWallet)
    : m_settings(settings), m_enclosingWallet(enclosingWallet) {
  assert(enclosingWallet != nullptr);

  if (settings.m_validating) {
    nIsValidatorEnabled = true;
  }
}

CAmount WalletExtension::GetStakeableBalance() const {
  LOCK2(cs_main, m_enclosingWallet->cs_wallet);

  CAmount balance = 0;

  for (const auto &it : m_enclosingWallet->mapWallet) {
    const CWalletTx &coin = it.second;
    if (coin.IsTrusted()) {
      balance += coin.GetAvailableCredit();
      balance += coin.GetAvailableWatchOnlyCredit();
    }
  }
  return balance;
}

bool WalletExtension::SetMasterKeyFromSeed(const key::mnemonic::Seed &seed,
                                           std::string &error) {
  const std::string walletFileName = m_enclosingWallet->GetName();
  const std::time_t currentTime = std::time(nullptr);
  std::string backupWalletFileName =
      walletFileName + "~" + std::to_string(currentTime);
  m_enclosingWallet->BackupWallet(backupWalletFileName);
  const CPubKey hdMasterKey = m_enclosingWallet->GenerateNewHDMasterKey(&seed);
  if (!m_enclosingWallet->SetHDMasterKey(hdMasterKey)) {
    error = "setting master key failed";
    return false;
  }
  if (!m_enclosingWallet->NewKeyPool()) {
    error = "could not generate new keypool";
    return false;
  }
  return true;
}

// UNIT-E: read validatorState from the wallet file
void WalletExtension::ReadValidatorStateFromFile() {
  if (m_settings.m_validating && !m_settings.m_proposing) {
    LogPrint(BCLog::FINALIZATION, "%s: -validating is enabled for wallet %s.\n",
             __func__, m_enclosingWallet->GetName());

    validatorState = ValidatorState();
    nIsValidatorEnabled = true;
  }
}

bool WalletExtension::SendDeposit(const CKeyID &keyID, CAmount amount,
                                  CWalletTx &wtxOut) {

  CCoinControl coinControl;
  CAmount nFeeRet;
  std::string sError;
  int nChangePosInOut = 1;
  std::vector<CRecipient> vecSend;

  CReserveKey reservekey(m_enclosingWallet);
  CPubKey pubKey;
  m_enclosingWallet->GetPubKey(keyID, pubKey);

  CRecipient r{CScript::CreatePayVoteSlashScript(pubKey), amount, true};
  vecSend.push_back(r);

  if (!m_enclosingWallet->CreateTransaction(
          vecSend, wtxOut, reservekey, nFeeRet, nChangePosInOut, sError,
          coinControl, true, TxType::DEPOSIT)) {

    LogPrint(BCLog::FINALIZATION, "%s: Cannot create deposit transaction.\n",
             __func__);
    return false;
  }

  {
    LOCK2(cs_main, m_enclosingWallet->cs_wallet);
    CValidationState state;
    if (!m_enclosingWallet->CommitTransaction(wtxOut, reservekey,
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

    if (validatorState.m_phase == +ValidatorState::Phase::NOT_VALIDATING) {
      LogPrint(BCLog::FINALIZATION,
               "%s: Validator waiting for deposit confirmation.\n", __func__);

      validatorState.m_phase =
          ValidatorState::Phase::WAITING_DEPOSIT_CONFIRMATION;
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

  CCoinControl coinControl;
  coinControl.m_fee_mode = FeeEstimateMode::CONSERVATIVE;

  wtxNewOut.fTimeReceivedIsTxTime = true;
  wtxNewOut.BindWallet(m_enclosingWallet);
  wtxNewOut.fFromMe = true;

  CReserveKey reservekey(m_enclosingWallet);
  CValidationState state;

  CMutableTransaction txNew;
  txNew.SetType(TxType::LOGOUT);

  if (validatorState.m_phase != +ValidatorState::Phase::IS_VALIDATING) {
    return error("%s: Cannot create logouts for non-validators.", __func__);
  }

  CTransactionRef prevTx = validatorState.m_lastEsperanzaTx;

  const CScript &scriptPubKey = prevTx->vout[0].scriptPubKey;
  CAmount amount = prevTx->vout[0].nValue;

  // We need to pay some minimal fees if we wanna make sure that the logout
  // will be included.
  FeeCalculation feeCalc;

  txNew.vin.push_back(
      CTxIn(prevTx->GetHash(), 0, CScript(), CTxIn::SEQUENCE_FINAL));

  CTxOut txout(amount, scriptPubKey);
  txNew.vout.push_back(txout);

  const auto nBytes =
      static_cast<unsigned int>(GetVirtualTransactionSize(txNew));

  const CAmount fees =
      GetMinimumFee(nBytes, coinControl, ::mempool, ::feeEstimator, &feeCalc);

  txNew.vout[0].nValue -= fees;

  CTransaction txNewConst(txNew);
  uint32_t nIn = 0;
  SignatureData sigdata;
  std::string strFailReason;

  if (!ProduceSignature(
          TransactionSignatureCreator(m_enclosingWallet, &txNewConst, nIn,
                                      amount, SIGHASH_ALL),
          scriptPubKey, sigdata, &txNewConst)) {
    return false;
  }
  UpdateTransaction(txNew, nIn, sigdata);

  wtxNewOut.SetTx(MakeTransactionRef(std::move(txNew)));

  {
    LOCK2(cs_main, m_enclosingWallet->cs_wallet);
    m_enclosingWallet->CommitTransaction(wtxNewOut, reservekey, g_connman.get(),
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

  CCoinControl coinControl;
  coinControl.m_fee_mode = FeeEstimateMode::CONSERVATIVE;

  wtxNewOut.fTimeReceivedIsTxTime = true;
  wtxNewOut.BindWallet(m_enclosingWallet);
  wtxNewOut.fFromMe = true;

  CReserveKey reservekey(m_enclosingWallet);
  CValidationState errState;
  CKeyID keyID = boost::get<CKeyID>(address);
  CPubKey pubKey;
  m_enclosingWallet->GetPubKey(keyID, pubKey);

  CMutableTransaction txNew;
  txNew.SetType(TxType::WITHDRAW);

  if (validatorState.m_phase == +ValidatorState::Phase::IS_VALIDATING) {
    return error("%s: Cannot withdraw with an active validator, logout first.",
                 __func__);
  }

  CTransactionRef prevTx = validatorState.m_lastEsperanzaTx;

  const std::vector<unsigned char> pkv = ToByteVector(pubKey.GetID());
  const CScript &scriptPubKey = CScript::CreateP2PKHScript(pkv);

  txNew.vin.push_back(
      CTxIn(prevTx->GetHash(), 0, CScript(), CTxIn::SEQUENCE_FINAL));

  // Calculate how much we have left of the initial withdraw
  CAmount initialDeposit = prevTx->vout[0].nValue;
  esperanza::FinalizationState *state =
      esperanza::FinalizationState::GetState();

  CAmount currentDeposit = 0;

  esperanza::Result res = state->CalculateWithdrawAmount(
      validatorState.m_validatorAddress, currentDeposit);

  if (res != +Result::SUCCESS) {
    LogPrint(BCLog::FINALIZATION, "%s: Cannot calculate withdraw amount: %s.\n",
             __func__, res._to_string());
    return false;
  }

  CAmount toWithdraw = std::min(currentDeposit, initialDeposit);

  CTxOut txout(toWithdraw, scriptPubKey);
  txNew.vout.push_back(txout);

  CAmount amountToBurn = initialDeposit - toWithdraw;

  if (amountToBurn > 0) {
    CTxOut burnTx(amountToBurn, CScript::CreateUnspendableScript());
    txNew.vout.push_back(burnTx);
  }

  // We need to pay some minimal fees if we wanna make sure that the withdraw
  // will be included.
  FeeCalculation feeCalc;

  const auto nBytes =
      static_cast<unsigned int>(GetVirtualTransactionSize(txNew));

  const CAmount fees =
      GetMinimumFee(nBytes, coinControl, ::mempool, ::feeEstimator, &feeCalc);

  txNew.vout[0].nValue -= fees;

  CTransaction txNewConst(txNew);
  uint32_t nIn = 0;
  SignatureData sigdata;

  if (!ProduceSignature(
          TransactionSignatureCreator(m_enclosingWallet, &txNewConst, nIn,
                                      initialDeposit, SIGHASH_ALL),
          scriptPubKey, sigdata, &txNewConst)) {
    return false;
  }
  UpdateTransaction(txNew, nIn, sigdata);

  wtxNewOut.SetTx(MakeTransactionRef(std::move(txNew)));

  m_enclosingWallet->CommitTransaction(wtxNewOut, reservekey, g_connman.get(),
                                       errState);
  if (errState.IsInvalid()) {
    LogPrint(BCLog::FINALIZATION,
             "%s: Cannot commit withdraw transaction: %s.\n", __func__,
             errState.GetRejectReason());
    return false;
  }

  return true;
}

void WalletExtension::VoteIfNeeded(const std::shared_ptr<const CBlock> &pblock,
                                   const CBlockIndex *blockIndex) {

  FinalizationState *state = FinalizationState::GetState(blockIndex);

  uint32_t dynasty = state->GetCurrentDynasty();

  if (dynasty >= validatorState.m_endDynasty) {
    return;
  }

  if (dynasty < validatorState.m_startDynasty) {
    return;
  }

  uint32_t epoch = FinalizationState::GetEpoch(blockIndex);

  // Avoid double votes
  if (validatorState.m_voteMap.find(epoch) != validatorState.m_voteMap.end()) {
    LogPrint(BCLog::FINALIZATION,
             "%s: Attampting to make a double vote for epoch %s.\n", __func__,
             epoch);
    return;
  }

  LogPrint(BCLog::FINALIZATION,
           "%s: Validator voting for epoch %d and dynasty %d.\n", __func__,
           epoch, dynasty);

  Vote vote = state->GetRecommendedVote(validatorState.m_validatorAddress);

  // Check for sorrounding votes
  if (vote.m_targetEpoch < validatorState.m_lastTargetEpoch ||
      vote.m_sourceEpoch < validatorState.m_lastSourceEpoch) {

    LogPrint(BCLog::FINALIZATION,
             "%s: Attampting to make a sorround vote, source: %s, target: %s"
             " prevSource %s, prevTarget: %s.\n",
             __func__, vote.m_sourceEpoch, vote.m_targetEpoch,
             validatorState.m_lastSourceEpoch,
             validatorState.m_lastTargetEpoch);
    return;
  }

  CWalletTx createdTx;
  CTransactionRef &prevRef = validatorState.m_lastEsperanzaTx;

  if (SendVote(prevRef, vote, createdTx)) {
    validatorState.m_voteMap[epoch] = vote;
    validatorState.m_lastTargetEpoch = vote.m_targetEpoch;
    validatorState.m_lastSourceEpoch = vote.m_sourceEpoch;

    LogPrint(BCLog::FINALIZATION, "%s: Casted vote with id %s.\n", __func__,
             createdTx.tx->GetHash().GetHex());
  }
}

/**
 *
 * \brief Creates a vote transaction starting from a Vote object and a previous
 * transaction (vote or deposit  reference. It fills inputs, outputs.
 * It does not support an address change between source and destination.
 *
 * \param[in] prevTxRef a reference to the initial DEPOSIT or previous VOTE
 * transaction, depending which one is the most recent
 * \param[in] vote the vote data
 * \param[out] wtxNew the vote transaction committed
 */
bool WalletExtension::SendVote(const CTransactionRef &prevTxRef,
                               const Vote &vote, CWalletTx &wtxNewOut) {

  wtxNewOut.fTimeReceivedIsTxTime = true;
  wtxNewOut.BindWallet(m_enclosingWallet);
  wtxNewOut.fFromMe = true;
  CReserveKey reservekey(m_enclosingWallet);
  CValidationState state;

  CMutableTransaction txNew;
  txNew.SetType(TxType::VOTE);

  if (validatorState.m_phase != +ValidatorState::Phase::IS_VALIDATING) {
    return error("%s: Cannot create votes for non-validators.", __func__);
  }

  const CScript &scriptPubKey = prevTxRef->vout[0].scriptPubKey;
  const CAmount amount = prevTxRef->vout[0].nValue;

  std::vector<unsigned char> voteSig;
  if (!esperanza::Vote::CreateSignature(m_enclosingWallet, vote, voteSig)) {
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
          TransactionSignatureCreator(m_enclosingWallet, &txNewConst, nIn,
                                      amount, SIGHASH_ALL),
          scriptPubKey, sigdata, &txNewConst)) {
    return false;
  }
  UpdateTransaction(txNew, nIn, sigdata);

  wtxNewOut.SetTx(MakeTransactionRef(std::move(txNew)));

  m_enclosingWallet->CommitTransaction(wtxNewOut, reservekey, g_connman.get(),
                                       state);
  if (state.IsInvalid()) {
    LogPrint(BCLog::FINALIZATION, "%s: Cannot commit vote transaction: %s.\n",
             __func__, state.GetRejectReason());
    return false;
  }

  return true;
}

void WalletExtension::BlockConnected(
    const std::shared_ptr<const CBlock> &pblock, const CBlockIndex *pindex) {

  LOCK2(cs_main, m_enclosingWallet->cs_wallet);
  if (nIsValidatorEnabled && !IsInitialBlockDownload()) {
    switch (validatorState.m_phase) {
      case ValidatorState::Phase::IS_VALIDATING: {
        VoteIfNeeded(pblock, pindex);

        // In case we are logged out, stop validating.
        FinalizationState *state = FinalizationState::GetState(pindex);
        uint32_t currentDynasty = state->GetCurrentDynasty();
        if (currentDynasty >= validatorState.m_endDynasty) {
          validatorState.m_phase = ValidatorState::Phase::NOT_VALIDATING;
        }
        break;
      }
      case ValidatorState::Phase::WAITING_DEPOSIT_FINALIZATION: {
        FinalizationState *state = FinalizationState::GetState(pindex);

        if (state->GetLastFinalizedEpoch() >= validatorState.m_depositEpoch) {
          // Deposit is finalized there is no possible rollback
          validatorState.m_phase = ValidatorState::Phase::IS_VALIDATING;

          const esperanza::Validator *validator =
              state->GetValidator(validatorState.m_validatorAddress);

          validatorState.m_startDynasty = validator->m_startDynasty;

          LogPrint(BCLog::FINALIZATION,
                   "%s: Validator's deposit finalized, the validator index "
                   "is %s.\n",
                   __func__, validatorState.m_validatorAddress.GetHex());
        }
        break;
      }
      default: { break; }
    }
  }
}

void WalletExtension::SignInput(CWalletTx *walletTx,
                                CMutableTransaction &mutableTx,
                                const unsigned int txInIndex) const {
  CTxIn &txIn = mutableTx.vin[txInIndex];
  const int prevOutIndex = txIn.prevout.n;
  const CTxOut &prevOut = walletTx->tx->vout[prevOutIndex];
  const CTransaction constTx(mutableTx);
  const CScript &prevOutScriptPubKey = prevOut.scriptPubKey;

  const ::TransactionSignatureCreator tsc(m_enclosingWallet, &constTx,
                                          txInIndex, prevOut.nValue);

  ::SignatureData signatureData;
  ::ProduceSignature(tsc, prevOutScriptPubKey, signatureData);
  ::UpdateTransaction(mutableTx, txInIndex, signatureData);
}

const proposer::State &WalletExtension::GetProposerState() const {
  return m_proposerState;
}

EncryptionState WalletExtension::GetEncryptionState() const {
  if (!m_enclosingWallet->IsCrypted()) {
    return EncryptionState::UNENCRYPTED;
  }
  if (m_enclosingWallet->IsLocked()) {
    return EncryptionState::LOCKED;
  }
  if (m_unlocked_for_staking_only) {
    return EncryptionState::UNLOCKED_FOR_STAKING_ONLY;
  }
  return EncryptionState::UNLOCKED;
}

bool WalletExtension::Unlock(
    const SecureString &wallet_passphrase, bool for_staking_only) {
  m_unlocked_for_staking_only = for_staking_only;
  return m_enclosingWallet->Unlock(wallet_passphrase);
}

}  // namespace esperanza
