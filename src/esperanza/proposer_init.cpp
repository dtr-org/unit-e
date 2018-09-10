// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/proposer_init.h>

#include <esperanza/proposer.h>

#include <util.h>

namespace esperanza {

std::unique_ptr<Proposer> proposer;

bool InitProposer(const Config& config, const std::vector<CWallet*>& wallets) {
  try {
    proposer.reset(new Proposer(config, wallets));
    return true;
  } catch (const std::runtime_error& exc) {
    LogPrintf("failed to create proposer threads: %s\n", exc.what());
    return false;
  }
}

void StartProposer() {
  LogPrint(BCLog::ESPERANZA, "starting proposer threads...\n");
 proposer->Start();
}

void StopProposer() {
  LogPrint(BCLog::ESPERANZA, "stopping proposer threads...\n");
  proposer->Stop();
  LogPrint(BCLog::ESPERANZA, "all proposer threads exited.\n");
}

}  // namespace esperanza
