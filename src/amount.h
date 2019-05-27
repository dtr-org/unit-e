// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_AMOUNT_H
#define UNITE_AMOUNT_H

#include <stdint.h>

//UNIT-E: revise both MAX_MONEY and terms (satoshi)
/** Amount in satoshis (Can be negative) */
typedef int64_t CAmount;

static const CAmount UNIT = 100000000;
static const CAmount EEES = 1000000;

/** No amount larger than this (in satoshi) is valid.
 *
 * Note that this constant is *not* the total money supply, which in Unit-e
 * currently happens to be less than 27,182,751,000 UTE for various reasons, but
 * rather a sanity check. As this sanity check is used by consensus-critical
 * validation code, the exact value of the MAX_MONEY constant is consensus
 * critical; in unusual circumstances like a(nother) overflow bug that allowed
 * for the creation of coins out of thin air modification could lead to a fork.
 * */
static const CAmount MAX_MONEY = 27182751000 * UNIT;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

#endif //  UNITE_AMOUNT_H
