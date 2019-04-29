// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ESPERANZA_SCRIPT_H
#define ESPERANZA_SCRIPT_H

class CTransaction;
class uint160;
class CPubKey;

namespace esperanza {
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

#endif
