// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STAKING_PROOF_OF_STAKE_H
#define UNIT_E_STAKING_PROOF_OF_STAKE_H

#include <primitives/block.h>
#include <primitives/transaction.h>
#include <pubkey.h>

#include <vector>

namespace staking {

//! \brief Extract the staking key from a P2WPKH witness stack.
//!
//! As per https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki#p2wpkh,
//! a P2WPKH transaction looks like this:
//!
//!    witness:      <signature> <pubkey>
//!    scriptSig:    (empty)
//!    scriptPubKey: 0 <20-byte-key-hash>
//!                  (0x0014{20-byte-key-hash})
//!
//! That is: The pubkey we're interested in is in stack[1]
//! (stack[0] is the signature).
std::vector<CPubKey> ExtractP2WPKHKeys(const CScriptWitness &);

//! \brief Extract the staking key from a P2WSH witness stack.
//!
//! As per https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki#p2wsh,
//! a P2WSH transaction looks like this:
//!
//!    witness:      0 <signature1> <1 <pubkey1> <pubkey2> 2 CHECKMULTISIG>
//!    scriptSig:    (empty)
//!    scriptPubKey: 0 <32-byte-hash>
//!                  (0x0020{32-byte-hash})
//!
//! "0" in the first witness item is actually empty (each item in the
//! stack is encoded using a var int and the data following, this item
//! will be encoded using the var int 0 with no data following).
//!
//! The script is just an example and it is serialized. So we need to
//! pop the script off the stack, deserialize it, and check what kind
//! of script it is in order to extract the signing key.
std::vector<CPubKey> ExtractP2WSHKeys(const CScriptWitness &);

//! \brief extracts the pubkeys stored in the staking inputs witness program.
//!
//! In case of P2WPKH this returns the one and only pubkey from the witness stack.
//! In case of a P2SH staking input it returns all the potential signing keys.
std::vector<CPubKey> ExtractBlockSigningKeys(const CTxIn &);

//! \brief extracts the pubkey stored in the staking transaction's witness program.
//!
//! Convenience wrapper: Picks the staking input of the blocks coinbase transaction
//! and forwards the call to ExtractBlockSigningKeys(const TxIn &).
std::vector<CPubKey> ExtractBlockSigningKeys(const CBlock &);

}

#endif //UNIT_E_STAKING_PROOF_OF_STAKE_H
