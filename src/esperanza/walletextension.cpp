// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/validation.h>
#include <esperanza/finalizationstate.h>
#include <esperanza/walletextension.h>
#include <net.h>
#include <primitives/txtype.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>
#include <util.h>

namespace esperanza {

WalletExtension::WalletExtension(::CWallet *enclosingWallet)
    : m_enclosingWallet(enclosingWallet) {
  assert(enclosingWallet != nullptr);
}

size_t WalletExtension::GetProposerThreadIndex() const {
  return m_stakeThreadIndex;
}

CAmount WalletExtension::GetStakeableBalance() const {
  // todo
  return 0;
}

void WalletExtension::AvailableCoinsForStaking(std::vector<::COutput> &vCoins,
                                               int64_t nTime,
                                               int nHeight) const {
  // todo
}

bool WalletExtension::SelectCoinsForStaking(
    int64_t nTargetValue, int64_t nTime, int nHeight,
    std::set<std::pair<const ::CWalletTx *, unsigned int>> &setCoinsRet,
    int64_t &nValueRet) const {
  // todo
  return false;
}

bool WalletExtension::CreateCoinStake(unsigned int nBits, int64_t nTime,
                                      int nBlockHeight, int64_t nFees,
                                      ::CMutableTransaction &txNew,
                                      ::CKey &key) {
  // todo
  return false;
}

bool WalletExtension::SignBlock(::CBlockTemplate *pblocktemplate, int nHeight,
                                int64_t nSearchTime) {
  return false;
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
void WalletExtension::CreateWalletFromFile() {
  if (gArgs.GetBoolArg("-validating", false) &&
      !gArgs.GetBoolArg("-staking", true)) {
    LogPrint(BCLog::ESPERANZA, "%s: -validating is enabled for wallet %s.\n",
             "ESPERANZA", m_enclosingWallet->GetName());

    validatorState = esperanza::ValidatorState();
    nIsValidatorEnabled = true;
  }
}

/**
 * Creates a deposit transaction from the given account and amount.
 *
 * @param[in] address the destination
 * @param[in] amount
 * @param[out] wtxOut the transaction created
 * @return true if the operation was succesful, false otherwise.
 */
bool WalletExtension::SendDeposit(const CTxDestination &address,
                                  const CAmount &amount,
                                  CWalletTx &wtxOut) {

  CCoinControl coinControl;
  CAmount nFeeRet;
  std::string sError;
  int nChangePosInOut = 1;
  std::vector<CRecipient> vecSend;

  CReserveKey reservekey(m_enclosingWallet);
  CKeyID keyID = boost::get<CKeyID>(address);
  CPubKey pubKey;
  m_enclosingWallet->GetPubKey(keyID, pubKey);

  CRecipient r{CScript::CreatePayVoteSlashScript(pubKey), amount, true};
  vecSend.push_back(r);

  if (!m_enclosingWallet->CreateTransaction(vecSend, wtxOut, reservekey,
                                            nFeeRet, nChangePosInOut, sError,
                                            coinControl, true, TxType::DEPOSIT)) {

    LogPrint(BCLog::ESPERANZA, "%s: Cannot create deposit transaction.\n", "ESPERANZA");
    return false;
  }

  CValidationState state;
  if (!m_enclosingWallet->CommitTransaction(wtxOut, reservekey, g_connman.get(), state)) {
    LogPrint(BCLog::ESPERANZA, "%s: Cannot commit deposit transaction.\n", "ESPERANZA");
    return false;
  }

  LogPrint(BCLog::ESPERANZA, "%s: Created new deposit transaction %s.\n", "ESPERANZA", wtxOut.GetHash().GetHex());

  {
    LOCK(m_enclosingWallet->cs_wallet);
    if (validatorState.m_phase == ValidatorState::ValidatorPhase::NOT_VALIDATING) {
      LogPrint(BCLog::ESPERANZA,
               "%s: Validator waiting for deposit confirmation.\n",
               "ESPERANZA");

      validatorState.m_phase = ValidatorState::ValidatorPhase::WAITING_DEPOSIT_CONFIRMATION;
    } else {
      LogPrintf(
          "ERROR: %s - Wrong state for validator state with deposit %s, %s "
          "expected.\n",
          "ESPERANZA", wtxOut.GetHash().GetHex(),
          "WAITING_DEPOSIT_CONFIRMATION");
    }
  }

  return true;
}

void WalletExtension::VoteIfNeeded(const std::shared_ptr<const CBlock> &pblock,
                                   const CBlockIndex *blockIndex) {

    esperanza::FinalizationState* esperanza = esperanza::FinalizationState::GetState(*blockIndex);
    uint32_t dynasty = esperanza->GetCurrentDynasty();

    if(dynasty > validatorState.m_endDynasty){
      return;
    }

    uint32_t epoch = esperanza::FinalizationState::GetEpoch(*blockIndex);

    //Avoid double votes
    if (validatorState.m_voteMap.find(epoch) != validatorState.m_voteMap.end()) {
      return;
    }

    LogPrint(BCLog::ESPERANZA, "%s: Validator voting for epoch %d and dynasty %d.\n",
             "ESPERANZA",
             epoch,
             dynasty);

    VoteData vote = esperanza->GetRecommendedVote(validatorState.m_validatorIndex);

    //Check for sorrounding votes
    if (vote.m_targetEpoch < validatorState.m_lastTargetEpoch ||
        vote.m_sourceEpoch < validatorState.m_lastSourceEpoch) {
      return;
    }

    CWalletTx createdTx;
    CTransactionRef& prevRef = validatorState.m_lastVotableTx;

    if(SendVote(prevRef, vote, createdTx)){

      validatorState.m_voteMap[epoch] = vote;
      validatorState.m_lastTargetEpoch = vote.m_targetEpoch;
      validatorState.m_lastSourceEpoch = vote.m_sourceEpoch;
      validatorState.m_lastVotableTx = createdTx.tx;
      LogPrint(BCLog::ESPERANZA, "%s: Casted vote with id %s.\n",
               "ESPERANZA",
               createdTx.tx->GetHash().GetHex() );
    }
}

/**
 *
 * Creates a vote transaction starting from a VoteData object and a previous
 * DEPOSIT reference. It fills inputs, outputs. It does not support
 * an address change between source and destination.
 *
 * @param[in] prevTx a reference to the initial DEPOSIT transaction
 * @param[in] vote the vote data
 * @param[out] wtxNew the vote transaction ready for commit
 */
bool WalletExtension::SendVote(const CTransactionRef &depositRef,
                               const VoteData &vote, CWalletTx &wtxNewOut) {
    CCoinControl coinControl;
    wtxNewOut.fTimeReceivedIsTxTime = true;
    wtxNewOut.BindWallet(m_enclosingWallet);
    wtxNewOut.fFromMe = true;
    CReserveKey reservekey(m_enclosingWallet);
    CValidationState state;

    CMutableTransaction txNew;
    txNew.SetType(TxType::VOTE);

    if (validatorState.m_phase != ValidatorState::ValidatorPhase::IS_VALIDATING) {
      return error("%s: Cannot add vote inputs for non-validators.", __func__);
    }

    CScript scriptSig = CScript::EncodeVoteData(vote);

    const CScript &scriptPubKey = depositRef->vout[0].scriptPubKey;
    const CAmount amount = depositRef->vout[0].nValue;

    txNew.vin.push_back(CTxIn(depositRef->GetHash(), 0, scriptSig, CTxIn::SEQUENCE_FINAL));

    CTxOut txout(amount, scriptPubKey);
    txNew.vout.push_back(txout);

    CTransaction txNewConst(txNew);
    uint32_t nIn = 0;
    SignatureData sigdata;
    std::string strFailReason;

    if (!ProduceSignature(TransactionSignatureCreator(m_enclosingWallet, &txNewConst, nIn, amount, SIGHASH_ALL), scriptPubKey, sigdata)) {
      strFailReason = _("Signing transaction failed");
      return false;
    } else {
      UpdateTransaction(txNew, nIn, sigdata);
    }

    // Embed the constructed transaction data in wtxNew.
    wtxNewOut.SetTx(MakeTransactionRef(std::move(txNew)));

    if (!m_enclosingWallet->CommitTransaction(wtxNewOut, reservekey, g_connman.get(), state)) {
      LogPrint(BCLog::ESPERANZA, "%s: Cannot commit vote transaction: %s.\n",
               "ESPERANZA", state.GetRejectReason());
      return false;
    }

  return true;
}

void WalletExtension::BlockConnected(const std::shared_ptr<const CBlock> &pblock, const CBlockIndex *pindex) {

  if (nIsValidatorEnabled && !IsInitialBlockDownload()) {
    ValidatorState::ValidatorPhase currentPhase;
    {
      LOCK(m_enclosingWallet->cs_wallet);
      currentPhase = validatorState.m_phase;
    }

    if (currentPhase == ValidatorState::ValidatorPhase::IS_VALIDATING) {
      VoteIfNeeded(pblock, pindex);
    } else if (currentPhase == ValidatorState::ValidatorPhase::WAITING_DEPOSIT_FINALIZATION) {
      FinalizationState *esperanza = FinalizationState::GetState(*pindex);

      if (esperanza->GetLastFinalizedEpoch() >= validatorState.m_depositEpoch) {
        // Deposit is finalized there is no possible rollback
        {
          LOCK(m_enclosingWallet->cs_wallet);
          validatorState.m_phase = ValidatorState::ValidatorPhase::IS_VALIDATING;

          LogPrint(
              BCLog::ESPERANZA,
              "%s: Validator's deposit finalized, the validator index is %s.\n",
              "ESPERANZA", validatorState.m_validatorIndex.GetHex());
        }
      }
    }
  }
}

}  // namespace esperanza
