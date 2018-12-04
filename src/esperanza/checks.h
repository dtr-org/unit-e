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

bool IsVoteExpired(const CTransaction &tx);

bool CheckDepositTransaction(CValidationState &errState, const CTransaction &tx,
                             const CBlockIndex *pindex = nullptr);

bool CheckVoteTransaction(CValidationState &errState, const CTransaction &tx,
                          const Consensus::Params &consensusParams,
                          const CBlockIndex *pindex = nullptr);

bool CheckSlashTransaction(CValidationState &errState, const CTransaction &tx,
                           const Consensus::Params &consensusParams,
                           const CBlockIndex *pindex = nullptr);

bool CheckLogoutTransaction(CValidationState &errState, const CTransaction &tx,
                            const Consensus::Params &consensusParams,
                            const CBlockIndex *pindex = nullptr);

bool CheckWithdrawTransaction(CValidationState &errState,
                              const CTransaction &tx,
                              const Consensus::Params &consensusParams,
                              const CBlockIndex *pindex = nullptr);

bool CheckAdminTransaction(CValidationState &state, const CTransaction &tx,
                           const CBlockIndex *pindex = nullptr);

//! \brief Extracts the validator index from the transaction if applicable.
//! The function supports only LOGOUT, DEPOSIT and WITHDRAW, anything else
//! will return false.
//! \param tx
//! \param validatorAddressOut
//! \return true if successful, false otherwise.
bool ExtractValidatorAddress(const CTransaction &tx,
                             uint160 &validatorAddressOut);

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_CHECKS_H
