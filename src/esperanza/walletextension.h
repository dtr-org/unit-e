// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_WALLETEXTENSION_H
#define UNITE_ESPERANZA_WALLETEXTENSION_H

#include <amount.h>
#include <dependency.h>
#include <esperanza/validatorstate.h>
#include <esperanza/walletextension_deps.h>
#include <esperanza/walletstate.h>
#include <finalization/vote_recorder.h>
#include <injector.h>
#include <key.h>
#include <key/mnemonic/mnemonic.h>
#include <miner.h>
#include <primitives/transaction.h>
#include <proposer/proposer.h>
#include <proposer/proposer_state.h>
#include <settings.h>
#include <staking/stakingwallet.h>

#include <cstddef>
#include <cstdint>
#include <vector>

class CWallet;
class CWalletDBWrapper;
class CWalletTx;
class COutput;
class CScheduler;
class CCoinControl;
class CReserveKey;
struct CRecipient;

namespace esperanza {

//! \brief Extends the Bitcoin Wallet with Esperanza Capabilities.
//!
//! The rationale behind this design decision is to keep up with developments
//! in bitcoin-core. The alterations done to wallet.h/wallet.cpp are kept to
//! a minimum. All extended functionality should be put here.
class WalletExtension : public staking::StakingWallet {

 private:
  //! The dependencies of the wallet extension.
  esperanza::WalletExtensionDeps m_dependencies;

  //! The wallet this extension is embedded in.
  CWallet &m_enclosing_wallet;

  //! a miminum amount (in satoshis) to keep (will not be used for staking).
  CAmount m_reserve_balance = 0;

  //! the state of proposing blocks from this wallet
  proposer::State m_proposer_state;

  //! whether an encrypted wallet is unlocked only for staking
  bool m_unlocked_for_staking_only = false;

  std::vector<std::pair<finalization::VoteRecord, finalization::VoteRecord>> pendingSlashings;

  void VoteIfNeeded(const FinalizationState &state);

  void ManagePendingSlashings();

  template <typename Callable>
  void ForEachStakeableCoin(Callable) const;

  //! Backup the enclosing wallet to a new file in the datadir, appending
  //! the current timestamp to avoid overwriting previous backups.
  bool BackupWallet();

  class ValidatorStateWatchWriter {
   public:
    ValidatorStateWatchWriter(WalletExtension &wallet)
        : m_wallet(wallet),
          m_initial_state(wallet.validatorState),
          m_state(wallet.validatorState) {}
    ~ValidatorStateWatchWriter();

   private:
    WalletExtension &m_wallet;
    boost::optional<ValidatorState> m_initial_state;
    boost::optional<ValidatorState> &m_state;
  };

 public:
  //! \brief non-intrusive extension of the bitcoin-core wallet.
  //!
  //! A WalletExtension requires an enclosing wallet which it extends.
  //! The esperanza::WalletExtension is befriended by CWallet so that it
  //! can access CWallet's guts.
  //!
  //! @param enclosingWallet The CWallet that this WalletExtension extends (must
  //! not be nullptr).
  WalletExtension(const esperanza::WalletExtensionDeps &, ::CWallet &enclosingWallet);

  // defined in staking::StakingWallet
  CCriticalSection &GetLock() const override;

  // defined in staking::StakingWallet
  CAmount GetReserveBalance() const override;

  // defined in staking::StakingWallet
  CAmount GetStakeableBalance() const override;

  //! \brief returns the balance that is being staked on other nodes.
  virtual CAmount GetRemoteStakingBalance() const;

  // defined in staking::StakingWallet
  staking::CoinSet GetStakeableCoins() const override;

  // defined in staking::StakingWallet
  boost::optional<CKey> GetKey(const CPubKey &) const override;

  // defined in staking::StakingWallet
  bool SignCoinbaseTransaction(CMutableTransaction &) override;

  // defined in staking::StakingWallet
  proposer::State &GetProposerState() override;

  bool SetMasterKeyFromSeed(const key::mnemonic::Seed &seed,
                            bool brand_new,
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

  //! \brief Creates a remote staking transaction for a given address.
  //!
  //! The arguments mirror the ones for CWallet::CreateTransaction.
  //! \returns true if the operation was successful, false otherwise.
  bool CreateRemoteStakingTransaction(const CRecipient &recipient, CWalletTx *wtx_out,
                                      CReserveKey *key_change_out, CAmount *fee_out,
                                      std::string *error_out,
                                      const CCoinControl &coin_control);

  bool AddToWalletIfInvolvingMe(const CTransactionRef &tx,
                                const CBlockIndex *pIndex);

  void ReadValidatorStateFromFile();
  void WriteValidatorStateToFile();

  void BlockConnected(const std::shared_ptr<const CBlock> &pblock,
                      const CBlockIndex &index);

  const proposer::State &GetProposerState() const;

  boost::optional<ValidatorState> validatorState = boost::none;
  bool nIsValidatorEnabled = false;

  EncryptionState GetEncryptionState() const;

  bool Unlock(const SecureString &wallet_passphrase, bool for_staking_only);

  void SlashingConditionDetected(
      const finalization::VoteRecord &vote1,
      const finalization::VoteRecord &vote2);

  void PostInitProcess(CScheduler &scheduler);
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_WALLETEXTENSION_H
