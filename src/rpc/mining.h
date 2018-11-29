// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_RPC_MINING_H
#define UNITE_RPC_MINING_H

#include <script/script.h>

#include <univalue.h>

#ifdef ENABLE_WALLET
/** Generate blocks (mine) */
UniValue generateBlocks(std::shared_ptr<CReserveScript> coinstakeScript, int nGenerate, uint64_t nMaxTries, bool keepScript);
#endif

/** Check bounds on a command line confirm target */
unsigned int ParseConfirmTarget(const UniValue& value);

#endif
