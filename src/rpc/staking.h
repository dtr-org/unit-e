// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_RPC_STAKING_H
#define UNIT_E_RPC_STAKING_H

#include <staking/staking_rpc.h>

class CRPCTable;

void RegisterStakingRPCCommands(CRPCTable &t);

#endif  // UNIT_E_RPC_STAKING_H
