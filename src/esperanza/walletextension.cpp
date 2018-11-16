#include <random>

// Copyright (c) 2018 The Unit-e developers
// Copyright (c) 2017-2018 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/walletextension.h>

#include <consensus/validation.h>
#include <esperanza/finalizationstate.h>
#include <net.h>
#include <policy/policy.h>
#include <primitives/txtype.h>
#include <script/standard.h>
#include <staking/kernel.h>
#include <staking/stakevalidation.h>
#include <util.h>
#include <utilfun.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/fees.h>
#include <wallet/wallet.h>

namespace esperanza {

//! UNIT-E: check necessity of this constant
static const unsigned int DEFAULT_BLOCK_MAX_SIZE = 1000000;

WalletExtension::WalletExtension(const Settings &settings,
                                 ::CWallet *enclosing_wallet)
    : m_settings(settings), m_enclosing_wallet(enclosing_wallet) {
  assert(enclosing_wallet != nullptr);

  if (settings.m_validating) {
    nIsValidatorEnabled = true;
    validatorState = ValidatorState();
  }
}

CAmount WalletExtension::GetStakeableBalance() const {
  LOCK2(cs_main, m_enclosing_wallet->cs_wallet);

  CAmount balance = 0;

  for (const auto &it : m_enclosing_wallet->mapWallet) {
    const CWalletTx &coin = it.second;
    if (coin.IsTrusted()) {
      balance += coin.GetAvailableCredit();
      balance += coin.GetAvailableWatchOnlyCredit();
    }
  }
  return balance;
}

void WalletExtension::AvailableCoinsForStaking(std::vector<::COutput> &vCoins) {
  vCoins.clear();

  // side effect: sets deepestTxnDepth - this is why this function is not
  // declared const (happens twice)
  m_deepest_transaction_depth = 0;

  {
    LOCK2(cs_main, m_enclosing_wallet->cs_wallet);

    int height = chainActive.Tip()->nHeight;
    int requiredDepth = std::min<int>(
        ::Params().GetEsperanza().GetStakeMinConfirmations() - 1, height / 2);

    for (const auto &it : m_enclosing_wallet->mapWallet) {
      const CWalletTx &coin = it.second;
      CTransactionRef tx = coin.tx;

      int depth = coin.GetDepthInMainChain();  // requires cs_main lock

      if (depth > m_deepest_transaction_depth) {
        // side effect: sets deepestTxnDepth - this is why this function is not
        // declared const (happens twice)
        m_deepest_transaction_depth = depth;
      }
      if (depth < requiredDepth) {
        continue;
      }
      const uint256 &wtxid = it.first;
      const auto numOutputs = static_cast<const unsigned int>(tx->vout.size());
      for (unsigned int i = 0; i < numOutputs; ++i) {
        const auto &txout = tx->vout[i];

        COutPoint kernel(wtxid, i);
        if (!staking::CheckStakeUnused(kernel) ||
            m_enclosing_wallet->IsSpent(wtxid, i) ||
            m_enclosing_wallet->IsLockedCoin(wtxid, i)) {
          continue;
        }

        const CScript &pscriptPubKey = txout.scriptPubKey;
        CKeyID keyID;
        if (!staking::ExtractStakingKeyID(pscriptPubKey, keyID)) {
          continue;
        }
        if (m_enclosing_wallet->HaveKey(keyID)) {
          vCoins.emplace_back(&coin, i, depth,
                              /* fSpendable */ true, /* fSolvable */ true,
                              /* fSaveIn */ true);
        }
      }
    }
  }

  shuffle(vCoins.begin(), vCoins.end(), std::mt19937(std::random_device()()));
}

static bool SelectCoinsForStaking(
    int64_t nTargetValue, std::vector<::COutput> &availableCoinsForStaking,
    std::set<std::pair<const ::CWalletTx *, unsigned int>> &setCoinsRet,
    int64_t &nValueRet) {
  setCoinsRet.clear();
  nValueRet = 0;

  for (auto &output : availableCoinsForStaking) {
    const CWalletTx *pcoin = output.tx;
    int index = output.i;

    // Stop if we've chosen enough inputs
    if (nValueRet >= nTargetValue) {
      break;
    }

    int64_t amount = pcoin->tx->vout[index].nValue;

    std::pair<const CWalletTx *, unsigned int> coin =
        std::make_pair(pcoin, index);

    if (amount >= nTargetValue) {
      // If input value is greater or equal to target then simply insert
      //    it into the current subset and exit
      setCoinsRet.insert(coin);
      nValueRet += amount;
      break;
    } else if (amount < nTargetValue + EEES) {
      setCoinsRet.insert(coin);
      nValueRet += amount;
    }
  }

  return nValueRet >= nTargetValue;
}

bool WalletExtension::CreateCoinStake(unsigned int nBits, int64_t nTime,
                                      int nBlockHeight, int64_t nFees,
                                      ::CMutableTransaction &txNew,
                                      ::CKey &keyOut) {
  CBlockIndex *pindexPrev = chainActive.Tip();
  arith_uint256 bnTargetPerCoinDay;
  bnTargetPerCoinDay.SetCompact(nBits);

  CAmount nBalance = GetStakeableBalance();
  if (nBalance <= m_reserve_balance) {
    return false;
  }

  // Choose coins to use
  std::vector<const CWalletTx *> vwtxPrev;
  std::set<std::pair<const CWalletTx *, unsigned int>> setCoins;
  CAmount nValueIn = 0;

  std::vector<::COutput> availableCoinsForStaking;
  AvailableCoinsForStaking(availableCoinsForStaking);
  if (!SelectCoinsForStaking(nBalance - m_reserve_balance,
                             availableCoinsForStaking, setCoins, nValueIn)) {
    return false;
  }

  CAmount nCredit = 0;
  CScript scriptPubKeyKernel;

  auto it = setCoins.begin();

  for (; it != setCoins.end(); ++it) {
    auto pcoin = *it;
    COutPoint prevoutStake = COutPoint(pcoin.first->GetHash(), pcoin.second);

    int64_t nBlockTime;
    if (staking::CheckKernel(pindexPrev, nBits, nTime, prevoutStake,
                             &nBlockTime)) {
      LOCK(m_enclosing_wallet->cs_wallet);
      // Found a kernel
      LogPrint(BCLog::PROPOSING, "%s: Kernel found.\n", __func__);

      const CTxOut &kernelOut = pcoin.first->tx->vout[pcoin.second];

      std::vector<std::vector<unsigned char>> vSolutions;
      txnouttype whichType;

      CScript pscriptPubKey = kernelOut.scriptPubKey;
      CScript coinstakePath;
      bool fConditionalStake = false;
      if (staking::HasIsCoinstakeOp(pscriptPubKey)) {
        fConditionalStake = true;
        if (!staking::GetCoinstakeScriptPath(pscriptPubKey, coinstakePath)) {
          continue;
        }
        pscriptPubKey = coinstakePath;
      }

      if (!Solver(pscriptPubKey, whichType, vSolutions)) {
        LogPrint(BCLog::PROPOSING, "%s: Failed to parse kernel.\n", __func__);
        break;
      }

      LogPrint(BCLog::PROPOSING, "%s: Parsed kernel type=%d.\n", __func__,
               whichType);
      CKeyID spendId;
      if (whichType == TX_PUBKEYHASH) {
        spendId = CKeyID(uint160(vSolutions[0]));
      } else {
        LogPrint(BCLog::PROPOSING, "%s: No support for kernel type=%d.\n",
                 __func__, whichType);
        break;  // only support pay to address (pay to pubkey hash)
      }

      if (!m_enclosing_wallet->GetKey(spendId, keyOut)) {
        LogPrint(BCLog::PROPOSING,
                 "%s: Failed to get key for kernel type=%d.\n", __func__,
                 whichType);
        break;  // unable to find corresponding key
      }

      if (fConditionalStake) {
        scriptPubKeyKernel = kernelOut.scriptPubKey;
      } else {
        scriptPubKeyKernel << OP_DUP << OP_HASH160 << ToByteVector(spendId)
                           << OP_EQUALVERIFY << OP_CHECKSIG;
      }

      // Ensure txn is empty
      txNew.vin.clear();
      txNew.vout.clear();

      // Mark as coin stake transaction
      txNew.SetVersion(1);  // UNIT-E: TODO decide transaction version
      txNew.SetType(TxType::COINSTAKE);

      txNew.vin.emplace_back(CTxIn(pcoin.first->GetHash(), pcoin.second));

      nCredit += kernelOut.nValue;
      vwtxPrev.push_back(pcoin.first);

      CTxOut out(0, scriptPubKeyKernel);
      txNew.vout.emplace_back(out);

      LogPrint(BCLog::PROPOSING, "%s: Added kernel.\n", __func__);

      setCoins.erase(it);
      break;
    }
  }

  if (nCredit == 0 || nCredit > nBalance - m_reserve_balance) {
    return false;
  }

  // Attempt to add more inputs
  // Only advantage here is to setup the next stake using this output as a
  // kernel to have a higher chance of staking
  size_t nStakesCombined = 0;
  it = setCoins.begin();
  while (it != setCoins.end()) {
    if (nStakesCombined >= m_proposer_settings->max_stake_combine) {
      break;
    }

    // Stop adding more inputs if already too many inputs
    if (txNew.vin.size() >= 100) {
      break;
    }

    // Stop adding more inputs if value is already pretty significant
    if (nCredit >= m_proposer_settings->stake_combine_threshold) {
      break;
    }

    auto itc = it++;  // copy the current iterator then increment it
    auto pcoin = *itc;

    const CTxOut &prevOut = pcoin.first->tx->vout[pcoin.second];

    // Only add coins of the same key/address as kernel
    if (prevOut.scriptPubKey != scriptPubKeyKernel) {
      continue;
    }
    // Stop adding inputs if reached reserve limit
    if (nCredit + prevOut.nValue > nBalance - m_reserve_balance) {
      break;
    }
    // Do not add additional significant input
    if (prevOut.nValue >= m_proposer_settings->stake_combine_threshold) {
      continue;
    }

    txNew.vin.emplace_back(pcoin.first->GetHash(), pcoin.second);
    nCredit += prevOut.nValue;
    vwtxPrev.push_back(pcoin.first);

    LogPrint(BCLog::PROPOSING, "%s: Combining kernel %s, %d.\n", __func__,
             pcoin.first->GetHash().ToString(), pcoin.second);
    nStakesCombined++;
    setCoins.erase(itc);
  }

  // Get block reward
  CAmount nReward =
      ::Params().GetEsperanza().GetProofOfStakeReward(pindexPrev, nFees);
  if (nReward < 0) {
    return false;
  }

  // Process development fund
  CAmount nRewardOut = nReward;

  // UNIT-E: Creating a reward to a rewardAddress has not been ported,
  // presumably belongs to coldstaking
  nCredit += nRewardOut;

  // Set output amount, split outputs if > nStakeSplitThreshold
  if (nCredit >= m_proposer_settings->stake_split_threshold) {
    CTxOut outSplit(0, scriptPubKeyKernel);

    txNew.vout.back().nValue = nCredit / 2;
    outSplit.nValue = nCredit - txNew.vout.back().nValue;
    txNew.vout.emplace_back(outSplit);
  } else {
    txNew.vout.back().nValue = nCredit;
  }

  // Create output for reward
  // UNIT-E: Creating a reward to a rewardAddress has not been ported,
  // presumably belongs to coldstaking

  // Sign
  int nIn = 0;
  for (const auto &pcoin : vwtxPrev) {
    uint32_t nPrev = txNew.vin[nIn].prevout.n;

    const CTxOut &prevOut = pcoin->tx->vout[nPrev];
    const CScript &scriptPubKeyOut = prevOut.scriptPubKey;

    SignatureData sigdata;
    CTransaction txToConst(txNew);
    if (!ProduceSignature(
            TransactionSignatureCreator(m_enclosing_wallet, &txToConst, nIn,
                                        prevOut.nValue, SIGHASH_ALL),
            scriptPubKeyOut, sigdata)) {
      return error("%s: ProduceSignature failed.", __func__);
    }

    UpdateTransaction(txNew, nIn, sigdata);
    nIn++;
  }

  // Limit size
  auto nBytes = static_cast<unsigned int>(
      ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION));
  if (nBytes >= DEFAULT_BLOCK_MAX_SIZE / 5) {
    return error("%s: Exceeded coinstake size limit.", __func__);
  }

  // Successfully generated coinstake
  return true;
}

bool WalletExtension::SetMasterKeyFromSeed(const key::mnemonic::Seed &seed,
                                           std::string &error) {
  const std::string walletFileName = m_enclosing_wallet->GetName();
  const std::time_t currentTime = std::time(nullptr);
  std::string backupWalletFileName =
      walletFileName + "~" + std::to_string(currentTime);
  m_enclosing_wallet->BackupWallet(backupWalletFileName);
  const CPubKey hdMasterKey = m_enclosing_wallet->GenerateNewHDMasterKey(&seed);
  if (!m_enclosing_wallet->SetHDMasterKey(hdMasterKey)) {
    error = "setting master key failed";
    return false;
  }
  if (!m_enclosing_wallet->NewKeyPool()) {
    error = "could not generate new keypool";
    return false;
  }
  return true;
}

// UNIT-E: read validatorState from the wallet file
void WalletExtension::ReadValidatorStateFromFile() {
  // UNIT-E: TODO temporary workaround to read from gArgs before properly injecting this
  bool proposing = gArgs.GetArg("-proposing", true);
  if (m_settings.m_validating && proposing) {
    LogPrint(BCLog::FINALIZATION, "%s: -validating is enabled for wallet %s.\n",
             __func__, m_enclosing_wallet->GetName());

    validatorState = ValidatorState();
    nIsValidatorEnabled = true;
  }
}

bool WalletExtension::SendDeposit(const CKeyID &keyID, CAmount amount,
                                  CWalletTx &wtxOut) {

  assert(validatorState);
  ValidatorState &validator = validatorState.get();

  CCoinControl coinControl;
  CAmount nFeeRet;
  std::string sError;
  int nChangePosInOut = 1;
  std::vector<CRecipient> vecSend;

  CReserveKey reservekey(m_enclosing_wallet);
  CPubKey pubKey;
  m_enclosing_wallet->GetPubKey(keyID, pubKey);

  CRecipient r{CScript::CreatePayVoteSlashScript(pubKey), amount, true};
  vecSend.push_back(r);

  if (!m_enclosing_wallet->CreateTransaction(
          vecSend, wtxOut, reservekey, nFeeRet, nChangePosInOut, sError,
          coinControl, true, TxType::DEPOSIT)) {

    LogPrint(BCLog::FINALIZATION, "%s: Cannot create deposit transaction.\n",
             __func__);
    return false;
  }

  {
    LOCK2(cs_main, m_enclosing_wallet->cs_wallet);
    CValidationState state;
    if (!m_enclosing_wallet->CommitTransaction(wtxOut, reservekey,
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
  ValidatorState &validator = validatorState.get();

  CCoinControl coinControl;
  coinControl.m_fee_mode = FeeEstimateMode::CONSERVATIVE;

  wtxNewOut.fTimeReceivedIsTxTime = true;
  wtxNewOut.BindWallet(m_enclosing_wallet);
  wtxNewOut.fFromMe = true;

  CReserveKey reservekey(m_enclosing_wallet);
  CValidationState state;

  CMutableTransaction txNew;
  txNew.SetType(TxType::LOGOUT);

  if (validator.m_phase != +ValidatorState::Phase::IS_VALIDATING) {
    return error("%s: Cannot create logouts for non-validators.", __func__);
  }

  CTransactionRef prevTx = validator.m_lastEsperanzaTx;

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

  if (!ProduceSignature(TransactionSignatureCreator(m_enclosing_wallet,
                                                    &txNewConst, nIn, amount,
                                                    SIGHASH_ALL),
                        scriptPubKey, sigdata, &txNewConst)) {
    return false;
  }
  UpdateTransaction(txNew, nIn, sigdata);

  wtxNewOut.SetTx(MakeTransactionRef(std::move(txNew)));

  {
    LOCK2(cs_main, m_enclosing_wallet->cs_wallet);
    m_enclosing_wallet->CommitTransaction(wtxNewOut, reservekey, g_connman.get(),
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
  ValidatorState &validator = validatorState.get();

  CCoinControl coinControl;
  coinControl.m_fee_mode = FeeEstimateMode::CONSERVATIVE;

  wtxNewOut.fTimeReceivedIsTxTime = true;
  wtxNewOut.BindWallet(m_enclosing_wallet);
  wtxNewOut.fFromMe = true;

  CReserveKey reservekey(m_enclosing_wallet);
  CValidationState errState;
  CKeyID keyID = boost::get<CKeyID>(address);
  CPubKey pubKey;
  m_enclosing_wallet->GetPubKey(keyID, pubKey);

  CMutableTransaction txNew;
  txNew.SetType(TxType::WITHDRAW);

  if (validator.m_phase == +ValidatorState::Phase::IS_VALIDATING) {
    return error("%s: Cannot withdraw with an active validator, logout first.",
                 __func__);
  }

  CTransactionRef prevTx = validator.m_lastEsperanzaTx;

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
      validator.m_validatorAddress, currentDeposit);

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
          TransactionSignatureCreator(m_enclosing_wallet, &txNewConst, nIn,
                                      initialDeposit, SIGHASH_ALL),
          scriptPubKey, sigdata, &txNewConst)) {
    return false;
  }
  UpdateTransaction(txNew, nIn, sigdata);

  wtxNewOut.SetTx(MakeTransactionRef(std::move(txNew)));

  m_enclosing_wallet->CommitTransaction(wtxNewOut, reservekey, g_connman.get(),
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

  assert(validatorState);
  ValidatorState &validator = validatorState.get();

  uint32_t dynasty = state->GetCurrentDynasty();

  if (dynasty >= validator.m_endDynasty) {
    return;
  }

  if (dynasty < validator.m_startDynasty) {
    return;
  }

  uint32_t epoch = FinalizationState::GetEpoch(blockIndex);

  // Avoid double votes
  if (validator.m_voteMap.find(epoch) != validator.m_voteMap.end()) {
    LogPrint(BCLog::FINALIZATION,
             "%s: Attampting to make a double vote for epoch %s.\n", __func__,
             epoch);
    return;
  }

  LogPrint(BCLog::FINALIZATION,
           "%s: Validator voting for epoch %d and dynasty %d.\n", __func__,
           epoch, dynasty);

  Vote vote = state->GetRecommendedVote(validator.m_validatorAddress);

  // Check for sorrounding votes
  if (vote.m_targetEpoch < validator.m_lastTargetEpoch ||
      vote.m_sourceEpoch < validator.m_lastSourceEpoch) {

    LogPrint(BCLog::FINALIZATION,
             "%s: Attampting to make a sorround vote, source: %s, target: %s"
             " prevSource %s, prevTarget: %s.\n",
             __func__, vote.m_sourceEpoch, vote.m_targetEpoch,
             validator.m_lastSourceEpoch, validator.m_lastTargetEpoch);
    return;
  }

  CWalletTx createdTx;
  CTransactionRef &prevRef = validator.m_lastEsperanzaTx;

  if (SendVote(prevRef, vote, createdTx)) {
    validator.m_voteMap[epoch] = vote;
    validator.m_lastTargetEpoch = vote.m_targetEpoch;
    validator.m_lastSourceEpoch = vote.m_sourceEpoch;

    LogPrint(BCLog::FINALIZATION, "%s: Casted vote with id %s.\n", __func__,
             createdTx.tx->GetHash().GetHex());
  }
}

bool WalletExtension::SendVote(const CTransactionRef &prevTxRef,
                               const Vote &vote, CWalletTx &wtxNewOut) {

  assert(validatorState);

  wtxNewOut.fTimeReceivedIsTxTime = true;
  wtxNewOut.BindWallet(m_enclosing_wallet);
  wtxNewOut.fFromMe = true;
  CReserveKey reservekey(m_enclosing_wallet);
  CValidationState state;

  CMutableTransaction txNew;
  txNew.SetType(TxType::VOTE);

  if (validatorState.get().m_phase != +ValidatorState::Phase::IS_VALIDATING) {
    return error("%s: Cannot create votes for non-validators.", __func__);
  }

  const CScript &scriptPubKey = prevTxRef->vout[0].scriptPubKey;
  const CAmount amount = prevTxRef->vout[0].nValue;

  std::vector<unsigned char> voteSig;
  if (!esperanza::Vote::CreateSignature(m_enclosing_wallet, vote, voteSig)) {
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

  if (!ProduceSignature(TransactionSignatureCreator(m_enclosing_wallet,
                                                    &txNewConst, nIn, amount,
                                                    SIGHASH_ALL),
                        scriptPubKey, sigdata, &txNewConst)) {
    return false;
  }
  UpdateTransaction(txNew, nIn, sigdata);

  wtxNewOut.SetTx(MakeTransactionRef(std::move(txNew)));

  m_enclosing_wallet->CommitTransaction(wtxNewOut, reservekey, g_connman.get(),
                                        state);
  if (state.IsInvalid()) {
    LogPrint(BCLog::FINALIZATION, "%s: Cannot commit vote transaction: %s.\n",
             __func__, state.GetRejectReason());
    return false;
  }

  return true;
}

bool WalletExtension::SendSlash(const CTransaction &prevTx,
                                const finalization::VoteRecord &vote1,
                                const finalization::VoteRecord &vote2) {

  CWalletTx slashTx;
  slashTx.fTimeReceivedIsTxTime = true;
  slashTx.BindWallet(m_enclosingWallet);
  slashTx.fFromMe = true;

  CMutableTransaction txNew;
  txNew.SetType(TxType::SLASH);

  CValidationState errState;

  CScript scriptSig = vote1.GetScript() + vote2.GetScript();

  const CScript burnScript = CScript::CreateUnspendableScript();

  txNew.vin.push_back(
      CTxIn(prevTx.GetHash(), 0, scriptSig, CTxIn::SEQUENCE_FINAL));

  CTxOut burnOut(prevTx.vout[0].nValue, burnScript);
  txNew.vout.push_back(burnOut);

  CTransaction txNewConst(txNew);
  uint32_t nIn = 0;
  SignatureData sigdata;

  if (!ProduceSignature(
          TransactionSignatureCreator(m_enclosingWallet, &txNewConst, nIn,
                                      burnOut.nValue, SIGHASH_ALL),
          burnScript, sigdata, &txNewConst)) {
    return false;
  }
  UpdateTransaction(txNew, nIn, sigdata);

  slashTx.SetTx(MakeTransactionRef(std::move(txNew)));

  CReserveKey reservekey(m_enclosingWallet);
  m_enclosingWallet->CommitTransaction(slashTx, reservekey, g_connman.get(),
                                       errState);
  if (errState.IsInvalid()) {
    LogPrint(BCLog::FINALIZATION, "%s: Cannot commit slash transaction: %s.\n",
             __func__, errState.GetRejectReason());
    return false;
  }

  return true;
}

void WalletExtension::BlockConnected(
    const std::shared_ptr<const CBlock> &pblock, const CBlockIndex *pindex) {

  LOCK2(cs_main, m_enclosing_wallet->cs_wallet);
  if (nIsValidatorEnabled && !IsInitialBlockDownload()) {

    assert(validatorState);

    switch (validatorState.get().m_phase) {
      case ValidatorState::Phase::IS_VALIDATING: {
        VoteIfNeeded(pblock, pindex);

        // In case we are logged out, stop validating.
        FinalizationState *state = FinalizationState::GetState(pindex);
        uint32_t currentDynasty = state->GetCurrentDynasty();
        if (currentDynasty >= validatorState.get().m_endDynasty) {
          validatorState.get().m_phase = ValidatorState::Phase::NOT_VALIDATING;
        }
        break;
      }
      case ValidatorState::Phase::WAITING_DEPOSIT_FINALIZATION: {
        FinalizationState *state = FinalizationState::GetState(pindex);

        if (state->GetLastFinalizedEpoch() >=
            validatorState.get().m_depositEpoch) {
          // Deposit is finalized there is no possible rollback
          validatorState.get().m_phase = ValidatorState::Phase::IS_VALIDATING;

          const esperanza::Validator *validator =
              state->GetValidator(validatorState.get().m_validatorAddress);

          validatorState.get().m_startDynasty = validator->m_startDynasty;

          LogPrint(BCLog::FINALIZATION,
                   "%s: Validator's deposit finalized, the validator index "
                   "is %s.\n",
                   __func__, validatorState.get().m_validatorAddress.GetHex());
        }
        break;
      }
      default: {
        break;
      }
    }
  }
}

const proposer::State &WalletExtension::GetProposerState() const {
  return m_proposer_state;
}

EncryptionState WalletExtension::GetEncryptionState() const {
  if (!m_enclosing_wallet->IsCrypted()) {
    return EncryptionState::UNENCRYPTED;
  }
  if (m_enclosing_wallet->IsLocked()) {
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
  return m_enclosing_wallet->Unlock(wallet_passphrase);
}

}  // namespace esperanza
