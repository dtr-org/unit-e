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
#include <staking/proof_of_stake.h>
#include <streams.h>
#include <validation.h>

#include <set>

namespace staking {

class StakeValidatorImpl : public StakeValidator {

  using Error = BlockValidationError;

 private:
  const Dependency<blockchain::Behavior> m_blockchain_behavior;
  const Dependency<ActiveChain> m_active_chain;

  mutable CCriticalSection m_cs;
  std::set<COutPoint> m_kernel_seen;

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
    if (block.vtx.empty()) {
      return BlockValidationResult(Error::NO_COINBASE_TRANSACTION);
    }
    const CTransactionRef coinbase_tx = block.vtx[0];
    if (coinbase_tx->GetType() != +TxType::COINBASE) {
      return BlockValidationResult(Error::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION);
    }
    // a valid coinbase transaction has a "meta" input at vin[0] and a staking
    // input at vin[1]. It may have more inputs which are combined in the coinbase
    // transaction, but only vin[1] determines the eligibility of the block. This
    // is necessary as a combination of coins would depend on the selection of these
    // coins and the system could be gamed by degrading it into a Proof-of-Work setting.
    if (coinbase_tx->vin.size() < 2) {
      return BlockValidationResult(Error::NO_STAKING_INPUT);
    }
    const CTxIn &staking_input = coinbase_tx->vin[1];
    const COutPoint staking_out_point = staking_input.prevout;
    const boost::optional<staking::Coin> stake = utxo_view.GetUTXO(staking_out_point);
    if (!stake) {
      LogPrint(BCLog::VALIDATION, "%s: Could not find coin for outpoint=%s\n", __func__, util::to_string(staking_out_point));
      return BlockValidationResult(Error::STAKE_NOT_FOUND);
    }
    const blockchain::Height height = stake->GetHeight();
    if (!IsStakeMature(height)) {
      LogPrint(BCLog::VALIDATION, "Immature stake found coin=%s height=%d\n", util::to_string(*stake), height);
      return BlockValidationResult(Error::STAKE_IMMATURE);
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
          LogPrint(BCLog::VALIDATION, "Letting artificial block generation succeed nevertheless (mine_blocks_on_demand=true)\n");
        } else {
          return BlockValidationResult(Error::STAKE_NOT_ELIGIBLE);
        }
      }
    }
    return CheckRemoteStakingOutputs(block.vtx[0], *stake, utxo_view);
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
        return BlockValidationResult(Error::TRANSACTION_INPUT_NOT_FOUND);
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
        return BlockValidationResult(Error::REMOTE_STAKING_INPUT_BIGGER_THAN_OUTPUT);
      }
    }
    return BlockValidationResult::success;
  }

 public:
  explicit StakeValidatorImpl(
      const Dependency<blockchain::Behavior> blockchain_behavior,
      const Dependency<ActiveChain> active_chain) : m_blockchain_behavior(blockchain_behavior),
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
    return staking::ComputeStakeModifier(
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
    return staking::ComputeKernelHash(
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
      return BlockValidationResult(Error::PREVIOUS_BLOCK_NOT_PART_OF_ACTIVE_CHAIN);
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
    const blockchain::Height stake_maturity_activation_height =
        m_blockchain_behavior->GetParameters().stake_maturity_activation_height;

    return chain_height <= stake_maturity_activation_height || at_depth > stake_maturity;
  }

 protected:
  blockchain::UTXOView &GetUTXOView() const override {
    return *m_active_chain;
  }
};

std::unique_ptr<StakeValidator> StakeValidator::New(
    const Dependency<blockchain::Behavior> blockchain_behavior,
    const Dependency<ActiveChain> active_chain) {
  return std::unique_ptr<StakeValidator>(new StakeValidatorImpl(
      blockchain_behavior,
      active_chain));
}

}  // namespace staking
