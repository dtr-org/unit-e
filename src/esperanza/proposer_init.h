// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_ESPERANZA_PROPOSER_INIT_H
#define UNIT_E_ESPERANZA_PROPOSER_INIT_H

#include <esperanza/settings.h>

#include <vector>

class CWallet;

namespace esperanza {

bool InitProposer(const Settings &settings,
                  const std::vector<CWallet *> &wallets);

void StartProposer();

void StopProposer();

void WakeProposer(const CWallet *wallet = nullptr);

}  // namespace esperanza

#endif  // UNIT_E_PROPOSER_INIT_H
