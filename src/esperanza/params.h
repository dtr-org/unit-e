// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNITE_ESPERANZA_PARAMS_H
#define UNITE_ESPERANZA_PARAMS_H

#include <amount.h>

#include <stdint.h>

namespace esperanza {

/*!
 * \brief Esperanza Proof-of-Stake specific blockchain parameters.
 */
class Params final {

 private:

  //! seconds to elapse before new modifier is computed
  uint32_t m_modifierInterval;

  //! min depth in chain before staked output is spendable
  uint32_t m_stakeMinConfirmations;

  //! targeted number of seconds between blocks
  uint32_t m_targetSpacing;

  uint32_t m_targetTimespan;

  //! bitmask of 4 bits, every kernel stake hash will change every 16 seconds
  uint32_t m_stakeTimestampMask = (1 << 4) - 1;

  uint32_t m_lastImportHeight;

 public:

  uint32_t GetModifierInterval() const;

  uint32_t GetStakeMinConfirmations() const;

  uint32_t GetTargetSpacing() const;

  uint32_t GetTargetTimespan() const;

  uint32_t GetStakeTimestampMask(int nHeight) const;

  uint32_t GetLastImportHeight() const;

};

} // namespace esperanza

#endif // UNITE_ESPERANZA_PARAMS_H
