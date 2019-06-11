// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_STAKING_PROOF_OF_STAKE_H
#define UNITE_STAKING_PROOF_OF_STAKE_H

#include <blockchain/blockchain_types.h>
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

//! \brief Computes the kernel hash which determines whether you're eligible for proposing or not.
//!
//! The kernel hash must not rely on the contents of the block as this would allow a proposer
//! to degrade the system into a PoW setting simply by selecting subsets of transactions to
//! include (this also allows a proposer to produce multiple eligible blocks with different
//! contents which is why detection of duplicate stake is crucial).
//!
//! At the same time the kernel hash must not be easily predictable, which is why some entropy
//! is added: The "stake modifier" is a value taken from a previous block.
//!
//! In case one is not eligible to propose: The cards are being reshuffled every so often,
//! which is why the "current time" (the block time of the block to propose) is part of the
//! computation for the kernel hash.
uint256 ComputeKernelHash(const uint256 &previous_block_stake_modifier,
                          blockchain::Time stake_block_time,
                          const uint256 &stake_txid,
                          std::uint32_t stake_out_index,
                          blockchain::Time target_block_time);

//! \brief Computes the stake modifier which is used to make the next kernel unpredictable.
//!
//! The stake modifier relies on the transaction hash of the coin staked and
//! the stake modifier of the previous block.
uint256 ComputeStakeModifier(const uint256 &stake_transaction_hash,
                             const uint256 &previous_blocks_stake_modifier);

}  // namespace staking

#endif  // UNITE_STAKING_PROOF_OF_STAKE_H
