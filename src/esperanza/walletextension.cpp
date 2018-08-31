#include <random>

// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017-2018 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/miner.h>
#include <esperanza/validation.h>
#include <esperanza/walletextension.h>
#include <utilfun.h>
#include <validation.h>
#include <wallet/wallet.h>

namespace esperanza {

WalletExtension::WalletExtension(::CWallet *enclosingWallet)
    : m_enclosingWallet(enclosingWallet) {
  assert(enclosingWallet != nullptr);
}

size_t WalletExtension::GetProposerThreadIndex() const {
  return m_stakeThreadIndex;
}

CAmount WalletExtension::GetStakeableBalance() const {
  LOCK2(cs_main, m_enclosingWallet->cs_wallet);

  CAmount balance = 0;

  for (const auto &it : m_enclosingWallet->mapWallet) {
    const CWalletTx &coin = it.second;
    if (coin.IsTrusted()) {
      balance += coin.GetAvailableCredit();
      balance += coin.GetAvailableWatchOnlyCredit();
    }
  }
  return balance;
}

void WalletExtension::AvailableCoinsForStaking(std::vector<::COutput> &vCoins) {
  vCoins.clear();

  // side effect: sets deepestTxnDepth - this is why this function is not
  // declared const (happens twice)
  m_deepestTxnDepth = 0;

  {
    LOCK2(cs_main, m_enclosingWallet->cs_wallet);

    int height = chainActive.Tip()->nHeight;
    int requiredDepth =
        std::min<int>(Params().GetStakeMinConfirmations() - 1, height / 2);

    for (const auto &it : m_enclosingWallet->mapWallet) {
      const CWalletTx &coin = it.second;
      CTransactionRef tx = coin.tx;

      int depth = coin.GetDepthInMainChain();  // requires cs_main lock

      if (depth > m_deepestTxnDepth) {
        // side effect: sets deepestTxnDepth - this is why this function is not
        // declared const (happens twice)
        m_deepestTxnDepth = depth;
      }
      if (depth < requiredDepth) {
        continue;
      }
      const uint256 &wtxid = it.first;
      const int numOutputs = static_cast<const int>(tx->vout.size());
      for (unsigned int i = 0; i < numOutputs; ++i) {
        const auto &txout = tx->vout[i];

        COutPoint kernel(wtxid, i);
        if (!CheckStakeUnused(kernel) || m_enclosingWallet->IsSpent(wtxid, i) ||
            m_enclosingWallet->IsLockedCoin(wtxid, i)) {
          continue;
        }

        const CScript &pscriptPubKey = txout.scriptPubKey;
        CKeyID keyID;
        if (!ExtractStakingKeyID(pscriptPubKey, keyID)) {
          continue;
        }
        if (m_enclosingWallet->HaveKey(keyID)) {
          vCoins.emplace_back(&coin, i, depth,
                              /* fSpendable */ true, /* fSolvable */ true,
                              /* fSaveIn */ true);
        }
      }
    }
  }

  shuffle(vCoins.begin(), vCoins.end(), std::mt19937(std::random_device()()));
}

bool WalletExtension::SelectCoinsForStaking(
    int64_t nTargetValue,
    std::vector<::COutput>& availableCoinsForStaking,
    std::set<std::pair<const ::CWalletTx *, unsigned int>> &setCoinsRet,
    int64_t &nValueRet) {

  setCoinsRet.clear();
  nValueRet = 0;

  for (auto &output : availableCoinsForStaking) {
    const CWalletTx *pcoin = output.tx;
    int index = output.i;

    // Stop if we've chosen enough inputs
    if (nValueRet >= nTargetValue) {
      break;
    }

    int64_t amount = pcoin->tx->vout[index].nValue;

    std::pair<const CWalletTx *, unsigned int> coin = std::make_pair(pcoin, index);

    if (amount >= nTargetValue) {
      // If input value is greater or equal to target then simply insert
      //    it into the current subset and exit
      setCoinsRet.insert(coin);
      nValueRet += amount;
      break;
    } else if (amount < nTargetValue + EEES) {
      setCoinsRet.insert(coin);
      nValueRet += amount;
    }
  }

  return nValueRet >= nTargetValue;
}

bool WalletExtension::CreateCoinStake(unsigned int nBits, int64_t nTime,
                                      int nBlockHeight, int64_t nFees,
                                      ::CMutableTransaction &txNew,
                                      ::CKey &key) {
  // todo
  return false;
}

bool WalletExtension::SignBlock(::CBlockTemplate *pblocktemplate, int nHeight,
                                int64_t nSearchTime) {
  return false;
}

bool WalletExtension::SetMasterKeyFromSeed(const key::mnemonic::Seed &seed,
                                           std::string &error) {
  const std::string walletFileName = m_enclosingWallet->GetName();
  const std::time_t currentTime = std::time(nullptr);
  std::string backupWalletFileName =
      walletFileName + "~" + std::to_string(currentTime);
  m_enclosingWallet->BackupWallet(backupWalletFileName);
  const CPubKey hdMasterKey = m_enclosingWallet->GenerateNewHDMasterKey(&seed);
  if (!m_enclosingWallet->SetHDMasterKey(hdMasterKey)) {
    error = "setting master key failed";
    return false;
  }
  if (!m_enclosingWallet->NewKeyPool()) {
    error = "could not generate new keypool";
    return false;
  }
  return true;
}

}  // namespace esperanza
