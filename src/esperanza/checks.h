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

//! \brief Generalized finalization transaction check. Asserts on non-finalization transactions.
bool CheckFinalizationTransaction(const CTransaction &tx, CValidationState &err_state,
                                  const Consensus::Params &params, const FinalizationState &fin_state);

bool CheckDepositTransaction(CValidationState &errState, const CTransaction &tx,
                             const FinalizationState &state);

bool CheckVoteTransaction(CValidationState &errState, const CTransaction &tx,
                          const Consensus::Params &consensusParams,
                          const FinalizationState &state);

bool CheckSlashTransaction(CValidationState &errState, const CTransaction &tx,
                           const Consensus::Params &consensusParams,
                           const FinalizationState &state);

bool CheckLogoutTransaction(CValidationState &errState, const CTransaction &tx,
                            const Consensus::Params &consensusParams,
                            const FinalizationState &state);

bool CheckWithdrawTransaction(CValidationState &errState,
                              const CTransaction &tx,
                              const Consensus::Params &consensusParams,
                              const FinalizationState &state);

bool CheckAdminTransaction(CValidationState &state, const CTransaction &tx,
                           const FinalizationState &finalizationState);

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
