// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_WALLETEXT_STAKINGWALLETEXTENSION_H
#define UNITE_ESPERANZA_WALLETEXT_STAKINGWALLETEXTENSION_H

#include <amount.h>
#include <miner.h>
#include <primitives/transaction.h>
#include <key.h>

#include <cstdint>
#include <cstddef>
#include <vector>

class CWallet;
class CWalletTx;
class COutput;

namespace esperanza {

namespace walletext {

/*!
 * \brief Extends the Bitcoin Wallet with Esperanza Staking Capabilities.
 */
class StakingWalletExtension {

  friend ::CWallet;

 private:

  //! The wallet this extension is embedded in.
  CWallet *m_enclosingWallet;

  //! The current state of this wallet with regards to staking.
  enum eStakingState {
    NOT_STAKING,
    IS_STAKING,
    NOT_STAKING_BALANCE,
    NOT_STAKING_DEPTH,
    NOT_STAKING_LOCKED,
    NOT_STAKING_LIMITED,
  } m_isStaking = NOT_STAKING;

  int64_t nLastCoinStakeSearchTime = 0;

  //! A miminum amount (in satoshis) to keep (will not be used for staking).
  int64_t m_reserveBalance;

  size_t m_numberOfStakeThreads = 0;

  int m_deepestTxnDepth = 0; // for stake mining

  int m_stakeLimitHeight = 0; // for regtest, don't stake above nStakeLimitHeight

  CAmount m_stakeCombineThreshold = 1000 * UNIT;

  CAmount m_stakeSplitThreshold = 2000 * UNIT;

  size_t m_MaxStakeCombine = 3;

  std::string m_rewardAddress;

  bool fUnlockForStakingOnly = false; // Use coldstaking instead

  StakingWalletExtension(::CWallet *enclosingWallet);

 public:

  bool SetReserveBalance(::CAmount nNewReserveBalance);

  uint64_t GetStakeWeight() const;

  void AvailableCoinsForStaking(std::vector<::COutput> &vCoins, int64_t nTime, int nHeight) const;

  bool SelectCoinsForStaking(int64_t nTargetValue,
                             int64_t nTime,
                             int nHeight,
                             std::set<std::pair<const ::CWalletTx *, unsigned int> > &setCoinsRet,
                             int64_t &nValueRet) const;

  bool CreateCoinStake(unsigned int nBits,
                       int64_t nTime,
                       int nBlockHeight,
                       int64_t nFees,
                       ::CMutableTransaction &txNew,
                       ::CKey &key);

  bool SignBlock(::CBlockTemplate *pblocktemplate, int nHeight, int64_t nSearchTime);

};

} // namespace walletext

} // namespace esperanza

#endif // UNITE_ESPERANZA_WALLETEXT_STAKINGWALLETEXTENSION_H
