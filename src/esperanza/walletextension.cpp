// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/walletextension.h>
#include <wallet/wallet.h>

namespace esperanza {

WalletExtension::WalletExtension(::CWallet *enclosingWallet)
    : m_enclosingWallet(enclosingWallet) {}

size_t WalletExtension::GetProposerThreadIndex() const {
  return m_stakeThreadIndex;
}

CAmount WalletExtension::GetStakeableBalance() const {
  // todo
  return 0;
}

void WalletExtension::AvailableCoinsForStaking(std::vector<::COutput> &vCoins,
                                               int64_t nTime,
                                               int nHeight) const {
  // todo
}

bool WalletExtension::SelectCoinsForStaking(
    int64_t nTargetValue, int64_t nTime, int nHeight,
    std::set<std::pair<const ::CWalletTx *, unsigned int>> &setCoinsRet,
    int64_t &nValueRet) const {
  // todo
  return false;
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
