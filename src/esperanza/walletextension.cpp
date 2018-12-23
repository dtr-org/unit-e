#include <random>

// Copyright (c) 2018 The Unit-e developers
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
#include <utilfun.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/fees.h>
#include <wallet/wallet.h>

#include <cstdint>

namespace esperanza {

CCriticalSection cs_pendingSlashing;

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

  for (const auto &it : m_enclosing_wallet.mapWallet) {
    const CWalletTx *const tx = &it.second;
    const uint256 &txId = tx->GetHash();
    const std::vector<::CTxOut> &coins = tx->tx->vout;
    const auto depth = static_cast<blockchain::Depth>(tx->GetDepthInMainChain());  // requires cs_main
    if (depth < 1) {
      // requires at least one confirmation
      continue;
    }
    for (std::size_t outix = 0; outix < coins.size(); ++outix) {
      if (m_enclosing_wallet.IsSpent(txId, outix)) {
        continue;
      }
      const CTxOut &coin = coins[outix];
      if (m_enclosing_wallet.GetCredit(coin, ISMINE_SPENDABLE) <= 0) {
        continue;
      }
      // UNIT-E TODO: Restrict to P2WPKH only once #212 is merged (fixes #48)
      if (!coin.scriptPubKey.IsPayToWitnessScriptHash() && !coin.scriptPubKey.IsPayToPublicKeyHash()) {
        continue;
      }
      f(tx, std::uint32_t(outix), depth);
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
  ForEachStakeableCoin([&](const CWalletTx *tx, std::uint32_t outIx, blockchain::Depth depth) {
    total_amount += tx->tx->vout[outIx].nValue;
  });
  return total_amount;
}

std::vector<staking::Coin> WalletExtension::GetStakeableCoins() const {
  std::vector<staking::Coin> coins;
  ForEachStakeableCoin([&](const CWalletTx *tx, std::uint32_t outIx, blockchain::Depth depth) {
    coins.emplace_back(staking::Coin{tx->tx->GetHash(), outIx, tx->tx->vout[outIx].nValue, depth});
  });
  return coins;
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

bool WalletExtension::SignCoinbaseTransaction(CMutableTransaction &tx) {
  AssertLockHeld(GetLock());

  const CTransaction tx_const(tx);
  unsigned int ix = 0;
  const auto &wallet = m_enclosing_wallet.mapWallet;
  for (std::size_t i = 1; i < tx.vin.size(); ++i) {  // skips the first input, which is the meta input
    const auto &input = tx.vin[i];
    const auto index = input.prevout.n;
    const auto mi = wallet.find(input.prevout.hash);
    const auto &vout = mi->second.tx->vout;
    if (mi == wallet.end() || index >= vout.size()) {
      return false;
    }
    const CTxOut &out = vout[index];
    SignatureData sigdata;
    const TransactionSignatureCreator sigcreator(&m_enclosing_wallet, &tx_const, ix, out.nValue, SIGHASH_ALL);
    if (!ProduceSignature(sigcreator, out.scriptPubKey, sigdata)) {
      return false;
    }
    UpdateTransaction(tx, ix, sigdata);
    ++ix;
  }
  return true;
}

bool WalletExtension::SetMasterKeyFromSeed(const key::mnemonic::Seed &seed,
                                           std::string &error) {
  const std::string walletFileName = m_enclosing_wallet.GetName();
  const std::time_t currentTime = std::time(nullptr);
  std::string backupWalletFileName =
      walletFileName + "~" + std::to_string(currentTime);
  m_enclosing_wallet.BackupWallet(backupWalletFileName);
  const CPubKey hdMasterKey = m_enclosing_wallet.GenerateNewHDMasterKey(&seed);
  if (!m_enclosing_wallet.SetHDMasterKey(hdMasterKey)) {
    error = "setting master key failed";
    return false;
  }
  if (!m_enclosing_wallet.NewKeyPool()) {
    error = "could not generate new keypool";
    return false;
  }
  return true;
}

// UNIT-E: read validatorState from the wallet file
void WalletExtension::ReadValidatorStateFromFile() {
  if (m_dependencies.settings->node_is_validator && !m_dependencies.settings->node_is_proposer) {
    LogPrint(BCLog::FINALIZATION, "%s: -validating is enabled for wallet %s.\n",
             __func__, m_enclosing_wallet.GetName());

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

  CTransactionRef prevTx = validator.m_lastEsperanzaTx;

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

  if (!ProduceSignature(TransactionSignatureCreator(&m_enclosing_wallet,
                                                    &txNewConst, nIn, amount,
                                                    SIGHASH_ALL),
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

  CTransactionRef prevTx = validator.m_lastEsperanzaTx;

  const CScript &prevScriptPubkey = prevTx->vout[0].scriptPubKey;

  txNew.vin.push_back(CTxIn(prevTx->GetHash(), 0, CScript(), CTxIn::SEQUENCE_FINAL));

  // Calculate how much we have left of the initial withdraw
  const CAmount initialDeposit = prevTx->vout[0].nValue;
  esperanza::FinalizationState *state = esperanza::FinalizationState::GetState();

  CAmount currentDeposit = 0;

  const esperanza::Result res = state->CalculateWithdrawAmount(
      validator.m_validatorAddress, currentDeposit);

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

  if (!ProduceSignature(
          TransactionSignatureCreator(&m_enclosing_wallet, &txNewConst, nIn,
                                      initialDeposit, SIGHASH_ALL),
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

void WalletExtension::VoteIfNeeded(const std::shared_ptr<const CBlock> &pblock,
                                   const CBlockIndex &blockIndex) {

  const FinalizationState &state = *FinalizationState::GetState(&blockIndex);

  assert(validatorState);
  ValidatorState &validator = validatorState.get();

  const uint32_t dynasty = state.GetCurrentDynasty();

  if (dynasty >= validator.m_endDynasty) {
    return;
  }

  if (dynasty < validator.m_startDynasty) {
    return;
  }

  const uint32_t epoch = state.GetEpoch(blockIndex);

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

  Vote vote = state.GetRecommendedVote(validator.m_validatorAddress);

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

  if (!ProduceSignature(TransactionSignatureCreator(&m_enclosing_wallet,
                                                    &txNewConst, nIn, amount,
                                                    SIGHASH_ALL),
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
  uint160 validatorAddress = vote1.vote.m_validatorAddress;
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

  auto sigCreator = TransactionSignatureCreator(&m_enclosing_wallet, &txNewConst, nIn,
                                                burnOut.nValue, SIGHASH_ALL);

  std::vector<unsigned char> vchSig;
  sigCreator.CreateSig(vchSig, pubKey.GetID(), burnOut.scriptPubKey, SIGVERSION_BASE);
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

    switch (validatorState.get().m_phase) {
      case ValidatorState::Phase::IS_VALIDATING: {
        VoteIfNeeded(pblock, index);

        // In case we are logged out, stop validating.
        FinalizationState *state = FinalizationState::GetState(&index);
        uint32_t currentDynasty = state->GetCurrentDynasty();
        if (currentDynasty >= validatorState.get().m_endDynasty) {
          validatorState.get().m_phase = ValidatorState::Phase::NOT_VALIDATING;
        }
        break;
      }
      case ValidatorState::Phase::WAITING_DEPOSIT_FINALIZATION: {
        FinalizationState *state = FinalizationState::GetState(&index);

        if (state->GetLastFinalizedEpoch() >= validatorState.get().m_depositEpoch) {
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

bool WalletExtension::AddToWalletIfInvolvingMe(const CTransactionRef &ptx,
                                               const CBlockIndex *pIndex) {

  const CTransaction &tx = *ptx;

  if (pIndex == nullptr) {
    return true;
  }

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

        state.m_validatorAddress = validatorAddress;
        state.m_lastEsperanzaTx = ptx;
        state.m_depositEpoch = esperanza::GetEpoch(*pIndex);

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
            finalizationState->GetValidator(state.m_validatorAddress);

        state.m_endDynasty = validator->m_endDynasty;
        state.m_lastEsperanzaTx = ptx;

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
        state.m_lastEsperanzaTx = ptx;
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
