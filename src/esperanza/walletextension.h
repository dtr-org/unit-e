// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_WALLETEXTENSION_H
#define UNITE_ESPERANZA_WALLETEXTENSION_H

#include <amount.h>
#include <dependency.h>
#include <esperanza/validatorstate.h>
#include <esperanza/walletstate.h>
#include <finalization/vote_recorder.h>
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
class CScheduler;

namespace esperanza {

//! \brief Extends the Bitcoin Wallet with Esperanza Capabilities.
//!
//! The rationale behind this design decision is to keep up with developments
//! in bitcoin-core. The alterations done to wallet.h/wallet.cpp are kept to
//! a minimum. All extended functionality should be put here.
class WalletExtension : public staking::StakingWallet {
  friend class proposer::ProposerImpl;

 private:
  //! a reference to the esperanza settings
  const Settings &m_settings;

  Dependency<proposer::Settings> m_proposer_settings;

  //! The wallet this extension is embedded in.
  CWallet *m_enclosing_wallet;

  //! a miminum amount (in satoshis) to keep (will not be used for staking).
  CAmount m_reserve_balance = 0;

  //! for selecting available coins for proposing
  int m_deepest_transaction_depth = 0;

  //! the state of proposing blocks from this wallet
  proposer::State m_proposer_state;

  //! whether an encrypted wallet is unlocked only for staking
  bool m_unlocked_for_staking_only = false;

  std::vector<std::pair<finalization::VoteRecord, finalization::VoteRecord>> pendingSlashings;

  void VoteIfNeeded(const std::shared_ptr<const CBlock> &pblock,
                    const CBlockIndex *pindex);

  void ManagePendingSlashings();

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

  bool SetMasterKeyFromSeed(const key::mnemonic::Seed &seed,
                            std::string &error);

  //! \brief Creates a deposit transaction for the given address and amount.
  //!
  //! \param[in] keyID the destination pubkey
  //! \param[in] amount
  //! \param[out] wtxOut the transaction created
  //! \returns true if the operation was successful, false otherwise.
  bool SendDeposit(const CKeyID &keyID, CAmount amount, CWalletTx &wtxOut);

  //! \brief Creates a vote transaction starting from a Vote object and a
  //! previous transaction (vote or deposit  reference. It fills inputs,
  //! outputs. It does not support an address change between source and
  //! destination.
  //!
  //! \param[in] prevTxRef a reference to the initial DEPOSIT or previous VOTE
  //! transaction, depending which one is the most recent
  //! \param[in] vote the vote data
  //! \param[out] wtxNew the vote transaction committed
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

  //! \brief Creates and send a slash transaction.
  //!
  //! \param vote1 the first vote extracted from the transaction.
  //! \param vote2 the second vote retrieved from the historic data.
  //! \returns true if the operation is succesful, false otehrwise.
  bool SendSlash(const finalization::VoteRecord &vote1,
                 const finalization::VoteRecord &vote2);

  bool AddToWalletIfInvolvingMe(const CTransactionRef &tx,
                                const CBlockIndex *pIndex);

  void ReadValidatorStateFromFile();

  void BlockConnected(const std::shared_ptr<const CBlock> &pblock,
                      const CBlockIndex *pindex);

  const proposer::State &GetProposerState() const;

  boost::optional<ValidatorState> validatorState = boost::none;
  bool nIsValidatorEnabled = false;

  EncryptionState GetEncryptionState() const;

  bool Unlock(const SecureString &wallet_passphrase, bool for_staking_only);

  void SlashingConditionDetected(const finalization::VoteRecord vote1, const finalization::VoteRecord vote2);

  void PostInitProcess(CScheduler &scheduler);
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_WALLETEXTENSION_H
