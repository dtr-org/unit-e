// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_TXTYPE_H
#define UNIT_E_TXTYPE_H


#include <stdint.h>

/*! \brief The type of a transaction (see CTransaction)
 *
 * In Bitcoin transactions have versions and are always of the same type. In UnitE transactions
 * have a version and a type as transactions can be one of different types. UnitE distinguishes
 * different types of transactions for implementing Proof-of-Stake.
 */
enum class TxType : uint16_t {
    STANDARD,
    COINSTAKE,
    DEPOSIT
};


#endif //UNIT_E_TXTYPE_H
