// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_WALLETEXT_STAKINGWALLETEXTENSION_H
#define UNITE_ESPERANZA_WALLETEXT_STAKINGWALLETEXTENSION_H

#include <amount.h>
#include <esperanza/stakingstate.h>
#include <key.h>
#include <key/mnemonic/mnemonic.h>
#include <miner.h>
#include <primitives/transaction.h>

#include <esperanza/stakethread.h>
#include <cstddef>
#include <cstdint>
#include <vector>

class CWallet;
class CWalletTx;
class COutput;

namespace esperanza {

/*!
 * \brief Extends the Bitcoin Wallet with Esperanza Staking Capabilities.
 */
class WalletExtension {
  friend class esperanza::StakeThread;

 private:
  //! The wallet this extension is embedded in.
  CWallet *m_enclosingWallet;

  //! The current state of this wallet with regards to staking.
  StakingState m_stakingState = StakingState::NOT_STAKING;

  int64_t m_lastCoinStakeSearchTime = 0;

  //! A miminum amount (in satoshis) to keep (will not be used for staking).
  int64_t m_reserveBalance;

  //! Which stake thread is mining on this wallet (max = uninitialized)
  size_t m_stakeThreadIndex = std::numeric_limits<size_t>::max();

  int m_deepestTxnDepth = 0;

  //! For regtest, don't stake above nStakeLimitHeight
  int m_stakeLimitHeight = 0;

  CAmount m_stakeCombineThreshold = 1000 * UNIT;

  CAmount m_stakeSplitThreshold = 2000 * UNIT;

  size_t m_MaxStakeCombine = 3;

  std::string m_rewardAddress;

 public:
  WalletExtension(::CWallet *enclosingWallet);

  size_t GetProposerThreadIndex() const;

  CAmount GetStakeableBalance() const;

  void AvailableCoinsForStaking(std::vector<::COutput> &vCoins, int64_t nTime,
                                int nHeight) const;

  bool SelectCoinsForStaking(
      int64_t nTargetValue, int64_t nTime, int nHeight,
      std::set<std::pair<const ::CWalletTx *, unsigned int>> &setCoinsRet,
      int64_t &nValueRet) const;

  bool CreateCoinStake(unsigned int nBits, int64_t nTime, int nBlockHeight,
                       int64_t nFees, ::CMutableTransaction &txNew,
                       ::CKey &key);

  bool SignBlock(::CBlockTemplate *pblocktemplate, int nHeight,
                 int64_t nSearchTime);

  bool SetMasterKeyFromSeed(const key::mnemonic::Seed &seed,
                            std::string &error);
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_WALLETEXT_STAKINGWALLETEXTENSION_H
