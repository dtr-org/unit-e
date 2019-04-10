// Copyright (c) 2017-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/tx_verify.h>

#include <consensus/consensus.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <consensus/validation.h>
#include <blockchain/blockchain_behavior.h>
#include <esperanza/vote.h>
#include <finalization/vote_recorder.h>
#include <proposer/finalization_reward_logic.h>

// TODO remove the following dependencies
#include <chain.h>
#include <coins.h>
#include <utilmoneystr.h>

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime)
{
    if (tx.nLockTime == 0)
        return true;
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    for (const auto& txin : tx.vin) {
        if (!(txin.nSequence == CTxIn::SEQUENCE_FINAL))
            return false;
    }
    return true;
}

std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    assert(prevHeights->size() == tx.vin.size());

    // Will be set to the equivalent height- and time-based nLockTime
    // values that would be necessary to satisfy all relative lock-
    // time constraints given our view of block chain history.
    // The semantics of nLockTime are the last invalid height/time, so
    // use -1 to have the effect of any height or time being valid.
    int nMinHeight = -1;
    int64_t nMinTime = -1;

    // tx.nVersion is signed integer so requires cast to unsigned otherwise
    // we would be doing a signed comparison and half the range of nVersion
    // wouldn't support BIP 68.
    bool fEnforceBIP68 = static_cast<uint32_t>(tx.nVersion) >= 2
                      && flags & LOCKTIME_VERIFY_SEQUENCE;

    // Do not enforce sequence numbers as a relative lock time
    // unless we have been instructed to
    if (!fEnforceBIP68) {
        return std::make_pair(nMinHeight, nMinTime);
    }

    for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
        const CTxIn& txin = tx.vin[txinIndex];

        // Sequence numbers with the most significant bit set are not
        // treated as relative lock-times, nor are they given any
        // consensus-enforced meaning at this point.
        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) {
            // The height of this input is not relevant for sequence locks
            (*prevHeights)[txinIndex] = 0;
            continue;
        }

        int nCoinHeight = (*prevHeights)[txinIndex];

        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) {
            int64_t nCoinTime = block.GetAncestor(std::max(nCoinHeight-1, 0))->GetMedianTimePast();
            // NOTE: Subtract 1 to maintain nLockTime semantics
            // BIP 68 relative lock times have the semantics of calculating
            // the first block or time at which the transaction would be
            // valid. When calculating the effective block time or height
            // for the entire transaction, we switch to using the
            // semantics of nLockTime which is the last invalid block
            // time or height.  Thus we subtract 1 from the calculated
            // time or height.

            // Time-based relative lock-times are measured from the
            // smallest allowed timestamp of the block containing the
            // txout being spent, which is the median time past of the
            // block prior.
            nMinTime = std::max(nMinTime, nCoinTime + (int64_t)((txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) << CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) - 1);
        } else {
            nMinHeight = std::max(nMinHeight, nCoinHeight + (int)(txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) - 1);
        }
    }

    return std::make_pair(nMinHeight, nMinTime);
}

bool EvaluateSequenceLocks(const CBlockIndex& block, std::pair<int, int64_t> lockPair)
{
    if (block.nHeight == 0) {
        return true;
    }
    assert(block.pprev);
    int64_t nBlockTime = block.pprev->GetMedianTimePast();
    if (lockPair.first >= block.nHeight || lockPair.second >= nBlockTime)
        return false;

    return true;
}

bool SequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    return EvaluateSequenceLocks(block, CalculateSequenceLocks(tx, flags, prevHeights, block));
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    for (const auto& txin : tx.vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (const auto& txout : tx.vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const Coin& coin = inputs.AccessCoin(tx.vin[i].prevout);
        assert(!coin.IsSpent());
        const CTxOut &prevout = coin.out;
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}

int64_t GetTransactionSigOpCost(const CTransaction& tx, const CCoinsViewCache& inputs, int flags)
{
    int64_t nSigOps = GetLegacySigOpCount(tx) * WITNESS_SCALE_FACTOR;

    if (flags & SCRIPT_VERIFY_P2SH) {
        nSigOps += GetP2SHSigOpCount(tx, inputs) * WITNESS_SCALE_FACTOR;
    }

    for (std::size_t i = tx.IsCoinBase() ? 1 : 0; i < tx.vin.size(); ++i)
    {
        const Coin& coin = inputs.AccessCoin(tx.vin[i].prevout);
        assert(!coin.IsSpent());
        const CTxOut &prevout = coin.out;
        nSigOps += CountWitnessSigOps(tx.vin[i].scriptSig, prevout.scriptPubKey, &tx.vin[i].scriptWitness, flags, tx.GetType());
    }
    return nSigOps;
}

bool CheckTransaction(const CTransaction &tx, CValidationState &errState)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return errState.DoS(10, false, REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return errState.DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty");
    // Size limits (this doesn't take the witness into account, as that hasn't been checked for malleability)
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT)
        return errState.DoS(100, false, REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    CAmount nValueOut = 0;
    for (const auto& txout : tx.vout)
    {
        if (txout.nValue < 0)
            return errState.DoS(100, false, REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return errState.DoS(100, false, REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return errState.DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge");
    }
    std::set<COutPoint> vInOutPoints;
    for (const auto& txin : tx.vin)
    {
        if (!vInOutPoints.insert(txin.prevout).second) {
            return errState.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-duplicate");
        }
    }

    if (tx.IsCoinBase())
    {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            return errState.DoS(100, false, REJECT_INVALID, "bad-cb-length");
    }
    else
    {
        for (const auto& txin : tx.vin)
            if (txin.prevout.IsNull())
                return errState.DoS(10, false, REJECT_INVALID, "bad-txns-prevout-null");
    }

    switch (tx.GetType()) {
        case TxType::DEPOSIT:
        case TxType::VOTE:
        case TxType::LOGOUT:
            if (!tx.vout[0].scriptPubKey.IsFinalizerCommitScript()) {
                return errState.DoS(100, false, REJECT_INVALID, "bad-finalizercommit-vout-script");
            }
        case TxType::REGULAR:
        case TxType::SLASH:
        case TxType::COINBASE:
        case TxType::WITHDRAW:
        case TxType::ADMIN:
            break;
    }

    return true;
}

bool Consensus::CheckTxInputs(const CTransaction& tx, CValidationState& state, const AccessibleCoinsView& inputs, const int nSpendHeight, CAmount& txfee, CAmount *inputs_amount)
{
    if (nSpendHeight == 0) {
        // the genesis block does not have any inputs and does not spend anything.
        // it does create the initial stake in the system though and would fail
        // validation with bad-cb-spends-too-much.
        return true;
    }

    // are the actual inputs available?
    if (!inputs.HaveInputs(tx)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-missingorspent", false,
                         strprintf("%s: inputs missing/spent", __func__));
    }

    CAmount nValueIn = 0;
    for (std::size_t i = tx.IsCoinBase() ? 1 : 0; i < tx.vin.size(); ++i) {
        const COutPoint &prevout = tx.vin[i].prevout;
        const Coin& coin = inputs.AccessCoin(prevout);
        assert(!coin.IsSpent());

        // If prev is coinbase, check that the reward is mature
        if (coin.IsImmatureCoinBaseReward(prevout.n, nSpendHeight)) {
            return state.Invalid(false,
                REJECT_INVALID, "bad-txns-premature-spend-of-coinbase-reward",
                strprintf("tried to spend coinbase at depth %d", nSpendHeight - coin.nHeight));
        }

        // Check for negative or overflow input values
        nValueIn += coin.out.nValue;
        if (!MoneyRange(coin.out.nValue) || !MoneyRange(nValueIn)) {
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputvalues-outofrange");
        }
    }

    if (inputs_amount) {
        *inputs_amount = nValueIn;
    }

    const CAmount value_out = tx.GetValueOut();
    // Coinbase outputs are validated in BlockRewardValidator::CheckBlockRewards
    if (!tx.IsCoinBase()) {
        // All non-coinbase transactions have to spend no more than their inputs. If they spend
        // less, the change is counted towards the fees which are included in the reward
        // of the coinbase transaction.
        if (nValueIn < value_out) {
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-belowout", false,
                             strprintf("value in (%s) < value out (%s)",
                                       FormatMoney(nValueIn),
                                       FormatMoney(value_out)));
        }
    }

    // Tally transaction fees
    const CAmount txfee_aux = nValueIn - value_out;
    if (!tx.IsCoinBase() && !MoneyRange(txfee_aux)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-outofrange");
    }

    txfee = txfee_aux;
    return true;
}

bool Consensus::CheckBlockRewards(const CTransaction &coinbase_tx, CValidationState &state, const CBlockIndex &index,
                                  CAmount input_amount, CAmount fees, blockchain::Behavior &behavior,
                                  proposer::FinalizationRewardLogic &finalization_rewards) {
    assert(MoneyRange(input_amount));

    const CBlockIndex &prev_block = *index.pprev;
    CAmount block_reward = fees + behavior.CalculateBlockReward(prev_block.nHeight);

    std::size_t num_reward_outputs = finalization_rewards.GetNumberOfRewardOutputs(prev_block.nHeight + 1) + 1;
    if (coinbase_tx.vout.size() < num_reward_outputs) {
        return state.DoS(100,
                         error("%s: too few coinbase outputs expected at least %d actual %d", __func__,
                               num_reward_outputs, coinbase_tx.vout.size()),
                         REJECT_INVALID, "bad-cb-finalization-reward");
    }
    if (num_reward_outputs > 1 && !(prev_block.pprev->nStatus & BLOCK_HAVE_DATA)) {
        // prev_block is a parent block of the snapshot which was used for ISD.
        // We do not have data for the ancestor blocks of prev_block.
        // TODO UNIT-E: implement proper validation of finalization rewards after ISD
        LogPrintf("WARNING: %s partial validation of finalization rewards, block hash=%s\n", __func__,
                  HexStr(index.GetBlockHash()));
        std::vector<CAmount> fin_rewards = finalization_rewards.GetFinalizationRewardAmounts(prev_block);
        for (std::size_t i = 0; i < fin_rewards.size(); ++i) {
            block_reward += fin_rewards[i];
            if (coinbase_tx.vout[i + 1].nValue != fin_rewards[i]) {
                return state.DoS(100, error("%s: incorrect finalization reward", __func__), REJECT_INVALID,
                                 "bad-cb-finalization-reward");
            }
        }
    } else if (num_reward_outputs > 1) {
        std::vector<std::pair<CScript, CAmount>> fin_rewards = finalization_rewards.GetFinalizationRewards(prev_block);
        for (std::size_t i = 0; i < fin_rewards.size(); ++i) {
            block_reward += fin_rewards[i].second;
            if (coinbase_tx.vout[i + 1].nValue != fin_rewards[i].second ||
                coinbase_tx.vout[i + 1].scriptPubKey != fin_rewards[i].first) {
                return state.DoS(100, error("%s: incorrect finalization reward", __func__), REJECT_INVALID,
                                 "bad-cb-finalization-reward");
            }
        }
    }

    if (coinbase_tx.GetValueOut() - input_amount > block_reward) {
        return state.DoS(100,
                         error("%s: coinbase pays too much (total output=%d total input=%d expected reward=%d )",
                               __func__, FormatMoney(coinbase_tx.GetValueOut()), FormatMoney(input_amount),
                               FormatMoney(block_reward)),
                         REJECT_INVALID, "bad-cb-amount");
    }

    // TODO UNIT-E: make the check stricter: if (coinbase_tx.GetValueOut() - input_amount < block_reward)
    if (coinbase_tx.GetValueOut() < input_amount) {
        return state.DoS(100,
                         error("%s: coinbase pays too little (total output=%d total input=%d expected reward=%d )",
                               __func__, FormatMoney(coinbase_tx.GetValueOut()), FormatMoney(input_amount),
                               FormatMoney(block_reward)),
                         REJECT_INVALID, "bad-cb-spends-too-little");
    }

    CAmount non_reward_out = 0;

    for (std::size_t i = num_reward_outputs; i < coinbase_tx.vout.size(); ++i) {
        non_reward_out += coinbase_tx.vout[i].nValue;
    }
    if (non_reward_out > input_amount) {
        return state.DoS(100,
                         error("%s: coinbase spends too much (non-reward output=%d total input=%d)", __func__,
                               FormatMoney(non_reward_out), FormatMoney(input_amount)),
                         REJECT_INVALID, "bad-cb-spends-too-much");
    }
    return true;
}
