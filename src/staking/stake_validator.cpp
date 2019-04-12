// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/stake_validator.h>

#include <blockchain/blockchain_types.h>
#include <chainparams.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <staking/active_chain.h>
#include <streams.h>
#include <validation.h>

#include <set>

namespace staking {

class StakeValidatorImpl : public StakeValidator {

 private:
  Dependency<blockchain::Behavior> m_blockchain_behavior;
  Dependency<ActiveChain> m_active_chain;

  mutable CCriticalSection m_cs;
  std::set<COutPoint> m_kernel_seen;

  //! \brief Computes the kernel hash which determines whether you're eligible for proposing or not.
  //!
  //! The kernel hash must not rely on the contents of the block as this would allow a proposer
  //! to degrade the system into a PoW setting simply by selecting subsets of transactions to
  //! include (this also allows a proposer to produce multiple eligible blocks with different
  //! contents which is why detection of duplicate stake is crucial).
  //!
  //! At the same time the kernel hash must not be easily predictable, which is why some entropy
  //! is added: The "stake modifier" is a value taken from a previous block.
  //!
  //! In case one is not eligible to propose: The cards are being reshuffled every so often,
  //! which is why the "current time" (the block time of the block to propose) is part of the
  //! computation for the kernel hash.
  uint256 ComputeKernelHash(const uint256 &previous_block_stake_modifier,
                            const blockchain::Time stake_block_time,
                            const uint256 &stake_txid,
                            const std::uint32_t stake_out_index,
                            const blockchain::Time target_block_time) const {

    ::CDataStream s(SER_GETHASH, 0);

    s << previous_block_stake_modifier;
    s << stake_block_time;
    s << stake_txid;
    s << stake_out_index;
    s << target_block_time;

    return Hash(s.begin(), s.end());
  }

  //! \brief Computes the stake modifier which is used to make the next kernel unpredictable.
  //!
  //! The stake modifier relies on the transaction hash of the coin staked and
  //! the stake modifier of the previous block.
  uint256 ComputeStakeModifier(const uint256 &stake_transaction_hash,
                               const uint256 &previous_blocks_stake_modifier) const {

    ::CDataStream s(SER_GETHASH, 0);

    s << stake_transaction_hash;
    s << previous_blocks_stake_modifier;

    return Hash(s.begin(), s.end());
  }

  //! \brief Checks the stake of the given block. The previous block has to be part of the active chain.
  //!
  //! Looks up the stake in the UTXO set, which needs to be available from the
  //! active chain (this can not be used to validate blocks on a fork). The
  //! UTXO set should be always available and consistent, during reorgs the
  //! chain is rolled back using undo data and at every point a check of stake
  //! should be possible.
  BlockValidationResult CheckStakeInternal(const CBlockIndex &previous_block,
                                           const CBlock &block,
                                           const blockchain::UTXOView &utxo_view,
                                           const CheckStakeFlags::Type flags) const {
    AssertLockHeld(m_active_chain->GetLock());
    BlockValidationResult result;

    if (block.vtx.empty()) {
      result.errors += BlockValidationError::NO_COINBASE_TRANSACTION;
      return result;
    }
    const CTransactionRef coinbase_tx = block.vtx[0];
    if (coinbase_tx->GetType() != +TxType::COINBASE) {
      result.errors += BlockValidationError::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION;
      return result;
    }
    // a valid coinbase transaction has a "meta" input at vin[0] and a staking
    // input at vin[1]. It may have more inputs which are combined in the coinbase
    // transaction, but only vin[1] determines the eligibility of the block. This
    // is necessary as a combination of coins would depend on the selection of these
    // coins and the system could be gamed by degrading it into a Proof-of-Work setting.
    if (coinbase_tx->vin.size() < 2) {
      result.errors += BlockValidationError::NO_STAKING_INPUT;
      return result;
    }
    const CTxIn &staking_input = coinbase_tx->vin[1];
    const COutPoint staking_out_point = staking_input.prevout;
    const boost::optional<staking::Coin> stake = utxo_view.GetUTXO(staking_out_point);
    if (!stake) {
      LogPrint(BCLog::VALIDATION, "%s: Could not find coin for outpoint=%s\n", __func__, util::to_string(staking_out_point));
      result.errors += BlockValidationError::STAKE_NOT_FOUND;
      return result;
    }
    const blockchain::Height height = stake->GetHeight();
    if (!IsStakeMature(height)) {
      LogPrint(BCLog::VALIDATION, "Immature stake found coin=%s height=%d\n", util::to_string(*stake), height);
      result.errors += BlockValidationError::STAKE_IMMATURE;
      return result;
    }
    if (!Flags::IsSet(flags, CheckStakeFlags::SKIP_ELIGIBILITY_CHECK)) {
      const uint256 kernel_hash = ComputeKernelHash(&previous_block, *stake, block.nTime);
      // There are two ways to get the height of a block - either by parsing it from the coinbase, or by looking
      // at the height of the preceding block and incrementing it by one. The latter is simpler, so we do that.
      const blockchain::Height target_height = static_cast<blockchain::Height>(previous_block.nHeight) + 1;
      const blockchain::Difficulty target_difficulty =
          m_blockchain_behavior->CalculateDifficulty(target_height, *m_active_chain);
      if (!CheckKernel(stake->GetAmount(), kernel_hash, target_difficulty)) {
        LogPrint(BCLog::VALIDATION, "Kernel hash does not meet target coin=%s kernel=%s target=%d\n",
                 util::to_string(*stake), util::to_string(kernel_hash), target_difficulty);
        if (m_blockchain_behavior->GetParameters().mine_blocks_on_demand) {
          LogPrint(BCLog::VALIDATION, "Letting artificial block generation succeed nevertheles (mine_blocks_on_demand=true)\n");
        } else {
          result.errors += BlockValidationError::STAKE_NOT_ELIGIBLE;
          return result;
        }
      }
    }
    // Adding an error should immediately have returned so we assert to have validated the stake.
    assert(result);
    return CheckRemoteStakingOutputs(coinbase_tx, *stake, utxo_view);
  }

  //! \brief Check remote-staking outputs of a coinbase transaction.
  //!
  //! If a coinbase transaction contains an input with a remote-staking
  //! scriptPubKey then at least the same amount MUST be sent back to the same
  //! scriptPubKey.
  BlockValidationResult CheckRemoteStakingOutputs(
      const CTransactionRef &coinbase_tx,
      const staking::Coin &stake,
      const blockchain::UTXOView &utxo_view) const {
    BlockValidationResult result;
    std::map<CScript, CAmount> remote_staking_amounts;
    // check staking input
    WitnessProgram wp;
    if (stake.GetScriptPubKey().ExtractWitnessProgram(wp) && wp.IsRemoteStaking()) {
      remote_staking_amounts[stake.GetScriptPubKey()] += stake.GetAmount();
    }
    // check remaining inputs
    for (std::size_t i = 2; i < coinbase_tx->vin.size(); ++i) {
      const COutPoint out = coinbase_tx->vin[i].prevout;
      const boost::optional<staking::Coin> utxo = utxo_view.GetUTXO(out);
      if (!utxo) {
        result.errors += BlockValidationError::TRANSACTION_INPUT_NOT_FOUND;
        return result;
      }
      if (utxo->GetScriptPubKey().ExtractWitnessProgram(wp) && wp.IsRemoteStaking()) {
        remote_staking_amounts[utxo->GetScriptPubKey()] += utxo->GetAmount();
      }
    }
    for (const auto &out : coinbase_tx->vout) {
      if (remote_staking_amounts.count(out.scriptPubKey) != 0) {
        // This does not underflow if the transaction passes CheckTransaction from consensus/tx_verify.h
        remote_staking_amounts[out.scriptPubKey] -= out.nValue;
      }
    }
    for (const auto &p : remote_staking_amounts) {
      if (p.second > 0) {
        result.errors += BlockValidationError::REMOTE_STAKING_INPUT_BIGGER_THAN_OUTPUT;
        return result;
      }
    }
    return result;
  }

 public:
  explicit StakeValidatorImpl(
      Dependency<blockchain::Behavior> blockchain_behavior,
      Dependency<ActiveChain> active_chain) : m_blockchain_behavior(blockchain_behavior),
                                              m_active_chain(active_chain) {}

  CCriticalSection &GetLock() override {
    return m_cs;
  }

  uint256 ComputeStakeModifier(const CBlockIndex *previous_block,
                               const staking::Coin &stake) const override {

    if (!previous_block) {
      // The genesis block does not have a preceding block.
      // Its stake modifier is simply 0.
      return uint256::zero;
    }
    return ComputeStakeModifier(
        stake.GetTransactionId(),
        previous_block->stake_modifier);
  }

  uint256 ComputeKernelHash(const CBlockIndex *previous_block,
                            const staking::Coin &coin,
                            const blockchain::Time target_block_time) const override {
    if (!previous_block) {
      // The genesis block does not have a preceding block. It also does not
      // reference any stake. Its kernel hash is simply 0. This has the nice
      // property of meeting any target difficulty.
      return uint256::zero;
    }
    return ComputeKernelHash(
        previous_block->stake_modifier,
        coin.GetBlockTime(),
        coin.GetTransactionId(),
        coin.GetOutputIndex(),
        target_block_time);
  }

  bool CheckKernel(const CAmount stake_amount,
                   const uint256 &kernel_hash,
                   const blockchain::Difficulty target_difficulty) const override {

    if (stake_amount <= 0) {
      return false;
    }

    arith_uint256 target_value;
    bool is_negative;
    bool is_overflow;

    target_value.SetCompact(target_difficulty, &is_negative, &is_overflow);

    if (is_negative || is_overflow || target_value == 0) {
      return false;
    }

    const arith_uint256 weight(static_cast<uint64_t>(stake_amount));

    target_value *= weight;

    return UintToArith256(kernel_hash) <= target_value;
  }

 protected:
  BlockValidationResult CheckStake(
      const CBlock &block,
      const blockchain::UTXOView &utxo_view,
      CheckStakeFlags::Type flags,
      BlockValidationInfo *validation_info) const override {
    AssertLockHeld(m_active_chain->GetLock());
    BlockValidationResult result;
    if (m_blockchain_behavior->IsGenesisBlock(block)) {
      // The genesis block does not stake anything.
      return result;
    }
    if (validation_info && validation_info->GetCheckStakeStatus()) {
      // short circuit in case the validation already happened
      return result;
    }
    const CBlockIndex *const tip = m_active_chain->GetBlockIndex(block.hashPrevBlock);
    if (!tip) {
      result.errors += BlockValidationError::PREVIOUS_BLOCK_NOT_PART_OF_ACTIVE_CHAIN;
      return result;
    }
    return CheckStakeInternal(*tip, block, utxo_view, flags);
  }

 public:
  bool IsPieceOfStakeKnown(const COutPoint &stake) const override {
    AssertLockHeld(m_cs);
    return m_kernel_seen.find(stake) != m_kernel_seen.end();
  }

  void RememberPieceOfStake(const COutPoint &stake) override {
    AssertLockHeld(m_cs);
    m_kernel_seen.emplace(stake);
  }

  void ForgetPieceOfStake(const COutPoint &stake) override {
    AssertLockHeld(m_cs);
    m_kernel_seen.erase(stake);
  }

  bool IsStakeMature(const blockchain::Height height) const override {
    AssertLockHeld(m_active_chain->GetLock());

    const blockchain::Depth at_depth = m_active_chain->GetDepth(height);
    const blockchain::Height chain_height = m_active_chain->GetHeight();
    const blockchain::Height stake_maturity = m_blockchain_behavior->GetParameters().stake_maturity;
    const blockchain::Height stake_maturity_threshold =
        m_blockchain_behavior->GetParameters().stake_maturity_threshold;

    return chain_height <= stake_maturity_threshold || at_depth > stake_maturity;
  }

 protected:
  blockchain::UTXOView &GetUTXOView() const override {
    return *m_active_chain;
  }
};

std::unique_ptr<StakeValidator> StakeValidator::New(
    Dependency<blockchain::Behavior> blockchain_behavior,
    Dependency<ActiveChain> active_chain) {
  return std::unique_ptr<StakeValidator>(new StakeValidatorImpl(
      blockchain_behavior,
      active_chain));
}

}  // namespace staking
