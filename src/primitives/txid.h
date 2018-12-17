// Copyright (c) 2018 The Bitcoin ABC developers
// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_PRIMITIVES_TXID_H
#define UNITE_PRIMITIVES_TXID_H

#include <uint256.h>

/**
 * A TxId is the identifier of a transaction. Currently identical to TxHash but
 * differentiated for type safety.
 */
struct TxId : public uint256 {
    explicit TxId() : uint256() {}
    explicit TxId(const uint256 &b) : uint256(b) {}
};


#endif // UNITE_PRIMITIVES_TXID_H
