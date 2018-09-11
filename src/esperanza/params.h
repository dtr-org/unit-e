// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_PARAMS_H
#define UNITE_ESPERANZA_PARAMS_H

#include <chain.h>
#include <stdint.h>

class CChainParams;

namespace esperanza {

/*!
 * \brief Esperanza Proof-of-Stake specific blockchain parameters.
 */
class Params final {

 private:
  //! chain params these params are embedded in
  const ::CChainParams *m_chainParams;

  //! seconds to elapse before new modifier is computed
  uint32_t m_modifierInterval;

  //! min depth in chain before staked output is spendable
  uint32_t m_stakeMinConfirmations;

  //! targeted number of seconds between blocks
  uint32_t m_targetSpacing;

  uint32_t m_targetTimespan;

  //! bitmask of 4 bits, every kernel stake hash will change every 16 seconds
  int64_t m_stakeTimestampMask = (1 << 4) - 1;

  int64_t m_coinYearReward = 2 * EEES;  // 2% per year

 public:
  Params(const CChainParams *chainParams);

  uint32_t GetModifierInterval() const;

  uint32_t GetStakeMinConfirmations() const;

  uint32_t GetTargetSpacing() const;

  uint32_t GetTargetTimespan() const;

  int64_t GetStakeTimestampMask() const;

  int64_t GetCoinYearReward(int64_t nTime) const;

  int64_t GetProofOfStakeReward(const CBlockIndex *pindexPrev,
                                int64_t nFees) const;
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_PARAMS_H
