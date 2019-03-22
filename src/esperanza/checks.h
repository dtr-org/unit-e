// Copyright (c) 2018 The Unit-e developers
// Copyright (c) 2017 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_CHECKS_H
#define UNITE_ESPERANZA_CHECKS_H

#include <chain.h>
#include <consensus/validation.h>
#include <primitives/block.h>
#include <primitives/transaction.h>

namespace esperanza {

class FinalizationState;

//! \brief Check if the vote is referring to an epoch before the last known
//! finalization.
//!
//! It assumes that the vote is well formed and in general parsable. It does not
//! make anycheck over the validity of the vote transaction.
//! \param tx transaction containing the vote.
//! \returns true if the vote is expired, false otherwise.
bool IsVoteExpired(const CTransaction &tx);

//! The Check-family functions do basic transaction verifications such as transaction
//! type, format, solvable, etc.
//! The ContextualCheck-family functions do full transaction verifications. In addition to basic
//! checks it tests transaction is consistent with its input and finalization state.

//! \brief Generalized finalization transaction check. Asserts on non-finalization transactions.
bool CheckFinalizerCommit(const CTransaction &tx, CValidationState &err_state);

//! \brief Generalized finalization transaction contextual check. Asserts on non-finalization transactions.
bool ContextualCheckFinalizerCommit(const CTransaction &tx,
                                    CValidationState &err_state,
                                    const Consensus::Params &params,
                                    const FinalizationState &fin_state);

bool CheckDepositTx(const CTransaction &tx, CValidationState &err_state,
                    uint160 *validator_address_out);
bool ContextualCheckDepositTx(const CTransaction &tx, CValidationState &err_state,
                              const FinalizationState &fin_state);

bool CheckVoteTx(const CTransaction &tx, CValidationState &err_state,
                 Vote *vote_out, std::vector<unsigned char> *vote_sig_out);
bool ContextualCheckVoteTx(const CTransaction &tx, CValidationState &err_state,
                           const Consensus::Params &consensus_params,
                           const FinalizationState &fin_state);

bool CheckSlashTx(const CTransaction &tx, CValidationState &err_state,
                  Vote *vote1_out, Vote *vote2_out);
bool ContextualCheckSlashTx(const CTransaction &tx, CValidationState &err_state,
                            const Consensus::Params &consensus_params,
                            const FinalizationState &fin_state);

bool CheckLogoutTx(const CTransaction &tx, CValidationState &err_state,
                   uint160 *out_validator_address);
bool ContextualCheckLogoutTx(const CTransaction &tx, CValidationState &err_state,
                             const Consensus::Params &consensus_params,
                             const FinalizationState &fin_state);

bool CheckWithdrawTx(const CTransaction &tx, CValidationState &err_state,
                     uint160 *out_validator_address);
bool ContextualCheckWithdrawTx(const CTransaction &tx, CValidationState &err_state,
                               const Consensus::Params &consensus_arams,
                               const FinalizationState &fin_state);

bool CheckAdminTx(const CTransaction &tx, CValidationState &err_state,
                  std::vector<CPubKey> *keys_out);
bool ContextualCheckAdminTx(const CTransaction &tx, CValidationState &err_state,
                            const FinalizationState &fin_state);

//! \brief Extracts the validator address from the transaction if applicable.
//!
//! The function supports only LOGOUT, DEPOSIT and WITHDRAW, anything else
//! will return false.
//! \param tx
//! \param validatorAddressOut
//! \return true if successful, false otherwise.
bool ExtractValidatorAddress(const CTransaction &tx,
                             uint160 &validatorAddressOut);

//! \brief Extracts the validator pubkey from the transaction if applicable.
//!
//! The function supports only VOTE as tx type, anything else will return false.
//! \param tx
//! \param pubkeyOut
//! \return true if successful, false otherwise.
bool ExtractValidatorPubkey(const CTransaction &tx,
                            CPubKey &pubkeyOut);

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_CHECKS_H
