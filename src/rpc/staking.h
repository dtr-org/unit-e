// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_RPC_STAKING_H
#define UNITE_RPC_STAKING_H

#include <staking/staking_rpc.h>

class CRPCTable;

void RegisterStakingRPCCommands(CRPCTable &t);

#endif  // UNITE_RPC_STAKING_H
