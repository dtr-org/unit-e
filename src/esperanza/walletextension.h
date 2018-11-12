// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_WALLETEXTENSION_H
#define UNITE_ESPERANZA_WALLETEXTENSION_H

#include <amount.h>
#include <dependency.h>
#include <esperanza/validatorstate.h>
#include <key.h>
#include <key/mnemonic/mnemonic.h>
#include <miner.h>
#include <primitives/transaction.h>
#include <proposer/proposer.h>
#include <proposer/proposer_settings.h>
#include <proposer/proposer_state.h>
#include <staking/stakingwallet.h>

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
class WalletExtension : public staking::StakingWallet {
  friend class proposer::Proposer;

 private:
  //! a reference to the esperanza settings
  const Settings &m_settings;

  Dependency<proposer::Settings> m_proposerSettings;

  //! The wallet this extension is embedded in.
  CWallet *m_enclosingWallet;

  //! a miminum amount (in satoshis) to keep (will not be used for staking).
  CAmount m_reserveBalance = 0;

  //! for selecting available coins for proposing
  int m_deepestTxnDepth = 0;

  //! the state of proposing blocks from this wallet
  proposer::State m_proposerState;

  void VoteIfNeeded(const std::shared_ptr<const CBlock> &pblock,
                    const CBlockIndex *pindex);

 public:
  //! \brief non-intrusive extension of the bitcoin-core wallet.
  //!
  //! A WalletExtension requires an enclosing wallet which it extends.
  //! The esperanza::WalletExtension is befriended by CWallet so that it
  //! can access CWallet's guts.
  //!
  //! @param enclosingWallet The CWallet that this WalletExtension extends (must
  //! not be nullptr).
  WalletExtension(const Settings &settings, ::CWallet *enclosingWallet);

  CAmount GetStakeableBalance() const override;

  void AvailableCoinsForStaking(std::vector<::COutput> &vCoins) override;

  bool CreateCoinStake(unsigned int nBits, int64_t nTime, int nBlockHeight,
                       int64_t nFees, ::CMutableTransaction &txNew,
                       ::CKey &keyOut) override;

  bool SetMasterKeyFromSeed(const key::mnemonic::Seed &seed,
                            std::string &error);

  //! \brief Creates a deposit transaction for the given address and amount.
  //!
  //! \param[in] keyID the destination pubkey
  //! \param[in] amount
  //! \param[out] wtxOut the transaction created
  //! \returns true if the operation was successful, false otherwise.
  bool SendDeposit(const CKeyID &keyID, CAmount amount, CWalletTx &wtxOut);

  bool SendVote(const CTransactionRef &depositRef, const Vote &vote,
                CWalletTx &wtxNewOut);

  //! \brief Creates and sends a logout transaction.
  //!
  //! \param wtxNewOut[out] the logout transaction created.
  //! \returns true if the operation was successful, false otherwise.
  bool SendLogout(CWalletTx &wtxNewOut);

  //! \brief Creates and sends a withdraw transaction.
  //!
  //! \param wtxNewOut[out] the withdraw transaction created.
  //! \returns true if the operation was successful, false otherwise.
  bool SendWithdraw(const CTxDestination &address, CWalletTx &wtxNewOut);

  void ReadValidatorStateFromFile();

  void BlockConnected(const std::shared_ptr<const CBlock> &pblock,
                      const CBlockIndex *pindex);

  const proposer::State &GetProposerState() const;

  ValidatorState validatorState;
  bool nIsValidatorEnabled = false;

  //! whether an encrypted wallet is unlocked only for staking
  bool m_staking_only = false;
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_WALLETEXTENSION_H
