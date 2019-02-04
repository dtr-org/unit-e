// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/stake_validator.h>

#include <blockchain/blockchain_types.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <streams.h>

#include <set>

namespace staking {

class StakeValidatorImpl : public StakeValidator {

 private:
  CCriticalSection m_cs;
  std::set<uint256> m_kernel_seen;

 public:
  StakeValidatorImpl() {}

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

    ::CDataStream s(SER_GETHASH, 0);

    s << previous_block->stake_modifier;
    s << previous_block->nTime;
    s << coin.txid;
    s << coin.index;
    s << time;

    return Hash(s.begin(), s.end());
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

  bool IsKernelKnown(const uint256 &kernel_hash) override {
    AssertLockHeld(m_cs);
    return m_kernel_seen.find(kernel_hash) != m_kernel_seen.end();
  }

  void RememberKernel(const uint256 &kernel_hash) override {
    AssertLockHeld(m_cs);
    m_kernel_seen.emplace(kernel_hash);
  }

  void ForgetKernel(const uint256 &kernel_hash) override {
    AssertLockHeld(m_cs);
    m_kernel_seen.erase(kernel_hash);
  }
};

std::unique_ptr<StakeValidator> StakeValidator::New() {
  return std::unique_ptr<StakeValidator>(new StakeValidatorImpl());
}

}  // namespace staking
