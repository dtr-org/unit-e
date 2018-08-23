// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNITE_ESPERANZA_MINER_H
#define UNITE_ESPERANZA_MINER_H

#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>
#include <primitives/block.h>
#include <esperanza/config.h>
#include <wallet/wallet.h>

namespace esperanza {

double GetPoSKernelPS();

bool ExtractStakingKeyID(const CScript &scriptPubKey, CKeyID &keyID);

bool CheckStake(CBlock *pblock);

bool ImportOutputs(CBlockTemplate *blocktemplate, int height);

} // namespace esperanza

#endif // UNITE_ESPERANZA_MINER_H
