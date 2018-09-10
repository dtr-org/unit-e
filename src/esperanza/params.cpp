// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/params.h>

#include <chainparams.h>
#include <util.h>
#include <utilmoneystr.h>

namespace esperanza {

Params::Params(const CChainParams *chainParams) : m_chainParams(chainParams) {
  assert(chainParams != nullptr);
}

uint32_t Params::GetModifierInterval() const { return m_modifierInterval; }

uint32_t Params::GetStakeMinConfirmations() const {
  return m_stakeMinConfirmations;
}

uint32_t Params::GetTargetSpacing() const { return m_targetSpacing; }

uint32_t Params::GetTargetTimespan() const { return m_targetTimespan; }

int64_t Params::GetStakeTimestampMask() const { return m_stakeTimestampMask; }

int64_t Params::GetCoinYearReward(int64_t nTime) const {
  static const int64_t nSecondsInYear = 365 * 24 * 60 * 60;

  if (m_chainParams->NetworkIDString() != "regtest") {
    // Y1 5%, Y2 4%, Y3 3%, Y4 2%, ... YN 2%
    int64_t nYearsSinceGenesis =
        (nTime - m_chainParams->GenesisBlock().nTime) / nSecondsInYear;

    if (nYearsSinceGenesis >= 0 && nYearsSinceGenesis < 3) {
      return (5 - nYearsSinceGenesis) * EEES;
    }
  }

  return m_coinYearReward;
}

int64_t Params::GetProofOfStakeReward(const CBlockIndex *pindexPrev,
                                      int64_t nFees) const {
  int64_t nSubsidy;

  nSubsidy = (pindexPrev->nMoneySupply / UNIT) *
             GetCoinYearReward(pindexPrev->nTime) /
             (365 * 24 * (60 * 60 / m_targetSpacing));

  if (LogAcceptCategory(BCLog::POS) &&
      gArgs.GetBoolArg("-printcreation", false))
    LogPrintf("GetProofOfStakeReward(): create=%s\n",
              FormatMoney(nSubsidy).c_str());

  return nSubsidy + nFees;
}

}  // namespace esperanza
