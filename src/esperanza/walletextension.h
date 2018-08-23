// Copyright (c) 2018 The unit-e core developers
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

//! \brief Extends the Bitcoin Wallet with Esperanza Capabilities.
//!
//! The rationale behind this design decision is to keep up with developments
//! in bitcoin-core. The alterations done to wallet.h/wallet.cpp are kept to
//! a minimum. All extended functionality should be put here.
//!
//! UNIT-E: TODO: Some of the state in here really is a ProposerState
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

  int m_deepestTxnDepth = 0;  // for stake mining

  int m_stakeLimitHeight =
      0;  // for regtest, don't stake above nStakeLimitHeight

  CAmount m_stakeCombineThreshold = 1000 * UNIT;

  CAmount m_stakeSplitThreshold = 2000 * UNIT;

  size_t m_MaxStakeCombine = 3;

  std::string m_rewardAddress;

 public:
  WalletExtension(::CWallet *enclosingWallet);

  size_t GetProposerThreadIndex() const;

  CAmount GetStakeableBalance() const;

  void AvailableCoinsForStaking(std::vector<::COutput> &vCoins);

  bool SelectCoinsForStaking(
      int64_t nTargetValue,
      std::set<std::pair<const ::CWalletTx *, unsigned int>> &setCoinsRet,
      int64_t &nValueRet);

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
