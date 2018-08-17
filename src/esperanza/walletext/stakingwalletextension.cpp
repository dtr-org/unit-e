// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/walletext/stakingwalletextension.h>

namespace esperanza {

namespace walletext {

StakingWalletExtension::StakingWalletExtension(::CWallet *enclosingWallet) : m_enclosingWallet(enclosingWallet) {}

bool StakingWalletExtension::SetReserveBalance(::CAmount nNewReserveBalance) {
  return false;
}

uint64_t StakingWalletExtension::GetStakeWeight() const {
  return 0;
}

void StakingWalletExtension::AvailableCoinsForStaking(std::vector<::COutput> &vCoins,
                                                      int64_t nTime,
                                                      int nHeight) const {
}

bool StakingWalletExtension::SelectCoinsForStaking(int64_t nTargetValue,
                                                   int64_t nTime,
                                                   int nHeight,
                                                   std::set<std::pair<const ::CWalletTx *, unsigned int> > &setCoinsRet,
                                                   int64_t &nValueRet) const {
  return false;
}

bool StakingWalletExtension::CreateCoinStake(unsigned int nBits,
                                             int64_t nTime,
                                             int nBlockHeight,
                                             int64_t nFees,
                                             ::CMutableTransaction &txNew,
                                             ::CKey &key) {
  return false;
}

bool StakingWalletExtension::SignBlock(::CBlockTemplate *pblocktemplate, int nHeight, int64_t nSearchTime) {
  return false;
}

} // namespace walletext

} // namespace esperanza
