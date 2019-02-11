// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SCRIPT_ISMINE_H
#define UNITE_SCRIPT_ISMINE_H

#include <script/standard.h>

#include <stdint.h>

class CKeyStore;
class CScript;

/** IsMine() return codes */
enum isminetype
{
    ISMINE_NO = 0,
    ISMINE_WATCH_ONLY = 1,
    ISMINE_SPENDABLE = 2,
    ISMINE_HW_DEVICE = 6, // 0b110, implies ISMINE_SPENDABLE
    ISMINE_ALL = ISMINE_WATCH_ONLY | ISMINE_SPENDABLE
};

/** used for bitflags of isminetype */
typedef uint8_t isminefilter;

isminetype IsMine(const CKeyStore& keystore, const CScript& scriptPubKey);
isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest);

//! Check if we are able to use an output with given script_pub_key as a stake
bool IsStakeableByMe(const CKeyStore &keystore, const CScript &script_pub_key);

//! Check if the output with the given script is staked on a remote node
//! (meaning
bool IsStakedRemotely(const CKeyStore &keystore, const CScript &script_pub_key);

#endif // UNITE_SCRIPT_ISMINE_H
