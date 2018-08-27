// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/params.h>

namespace esperanza {

uint32_t Params::GetModifierInterval() const { return m_modifierInterval; }

uint32_t Params::GetStakeMinConfirmations() const {
  return m_stakeMinConfirmations;
}

uint32_t Params::GetTargetSpacing() const { return m_targetSpacing; }

uint32_t Params::GetTargetTimespan() const { return m_targetTimespan; }

uint32_t Params::GetStakeTimestampMask(int nHeight) const {
  return m_stakeTimestampMask;
}

uint32_t Params::GetLastImportHeight() const { return m_lastImportHeight; }

}  // namespace esperanza
