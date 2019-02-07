// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/stake_validator.h>

#include <blockchain/blockchain_types.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <staking/active_chain.h>
#include <streams.h>

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
  //! is added: The "stake modifier" is a value taken from a previous block, and the "entropy time"
  //! is the block time of that block.
  //!
  //! In case one is not eligible to propose: The cards are being reshuffled every so often,
  //! which is why the "current time" (the block time of the block to propose) is part of the
  //! computation for the kernel hash.
  uint256 ComputeKernelHash(const uint256 &stake_modifier,
                            const blockchain::Time entropy_time,
                            const uint256 &stake_txid,
                            const std::uint32_t stake_index,
                            const blockchain::Time current_time) const {

    ::CDataStream s(SER_GETHASH, 0);

    s << stake_modifier;
    s << entropy_time;
    s << stake_txid;
    s << stake_index;
    s << current_time;

    return Hash(s.begin(), s.end());
  }

  BlockValidationResult CheckStake(const CBlockIndex &previous_block, const CBlock &block) const {
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
    if (coinbase_tx->vin.size() < 2) {
      result.errors += BlockValidationError::NO_STAKING_INPUT;
      return result;
    }
    const CTxIn &staking_input = coinbase_tx->vin[1];
    const COutPoint staking_out_point = staking_input.prevout;
    const boost::optional<staking::Coin> stake = m_active_chain->GetUTXO(staking_out_point);
    if (!stake) {
      result.errors += BlockValidationError::STAKE_NOT_FOUND;
      return result;
    }
    if (!m_blockchain_behavior->IsStakeMature(stake->depth)) {
      result.errors += BlockValidationError::STAKE_IMMATURE;
      return result;
    }
    const uint256 kernel_hash = ComputeKernelHash(&previous_block, *stake, block.nTime);
    // There are two ways to get the height of a block - either by parsing it from the coinbase, or by looking
    // at the height of the preceding block and incrementing it by one. The latter is simpler, so we do that.
    const blockchain::Height target_height = static_cast<blockchain::Height>(previous_block.nHeight) + 1;
    const blockchain::Difficulty target_difficulty =
        m_blockchain_behavior->CalculateDifficulty(target_height, *m_active_chain);
    if (!CheckKernel(stake->amount, kernel_hash, target_difficulty)) {
      result.errors += BlockValidationError::STAKE_NOT_ELIGIBLE;
      return result;
    }
    // Adding an error should immediately have returned so we assert to have validated the stake.
    assert(result);
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
                               const uint256 &kernel_hash) const override {

    if (!previous_block) {
      // The genesis block does not have a preceding block.
      // Its stake modifier is simply 0.
      return uint256();
    }

    ::CDataStream s(SER_GETHASH, 0);

    s << kernel_hash;
    s << previous_block->stake_modifier;

    return Hash(s.begin(), s.end());
  }

  uint256 ComputeKernelHash(const CBlockIndex *previous_block,
                            const staking::Coin &coin,
                            const blockchain::Time time) const override {
    if (!previous_block) {
      // The genesis block does not have a preceding block. It also does not
      // reference any stake. Its kernel hash is simply 0. This has the nice
      // property of meeting any target difficulty.
      return uint256();
    }
    return ComputeKernelHash(
        previous_block->stake_modifier,
        previous_block->nTime,
        coin.txid,
        coin.index,
        time);
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

  BlockValidationResult CheckStake(const CBlock &block) const override {
    BlockValidationResult result;
    if (m_blockchain_behavior->IsGenesisBlock(block)) {
      // The genesis block does not stake anything.
      return result;
    }
    const CBlockIndex *const tip = m_active_chain->GetBlockIndex(block.hashPrevBlock);
    if (!tip) {
      result.errors += BlockValidationError::PREVIOUS_BLOCK_NOT_PART_OF_ACTIVE_CHAIN;
      return result;
    }
    return CheckStake(*tip, block);
  }

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
};

std::unique_ptr<StakeValidator> StakeValidator::New(
    Dependency<blockchain::Behavior> blockchain_behavior,
    Dependency<ActiveChain> active_chain) {
  return std::unique_ptr<StakeValidator>(new StakeValidatorImpl(
      blockchain_behavior,
      active_chain));
}

}  // namespace staking
