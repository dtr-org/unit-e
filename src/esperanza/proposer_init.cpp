// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/proposer_init.h>

#include <esperanza/proposer.h>
#include <util.h>

#include <mutex>

namespace esperanza {

static std::mutex initLock;
static std::unique_ptr<Proposer> proposer = nullptr;

bool InitProposer(const Settings &settings,
                  const std::vector<CWallet *> &wallets) {
  std::unique_lock<decltype(initLock)> lock;
  if (proposer) {
    return false;
  }
  if (!settings.m_proposing) {
    LogPrint(BCLog::FINALIZATION,
             "not starting proposer, proposing is not activated.\n");
    return true;
  }
  try {
    proposer.reset(new Proposer(settings, wallets));
    return true;
  } catch (const std::runtime_error &exc) {
    LogPrint(BCLog::FINALIZATION, "failed to create proposer threads: %s\n",
             exc.what());
    return false;
  }
}

void StartProposer() {
  if (proposer) {
    LogPrint(BCLog::FINALIZATION, "starting proposer threads...\n");
    proposer->Start();
  }
}

void StopProposer() {
  if (proposer) {
    LogPrint(BCLog::FINALIZATION, "stopping proposer threads...\n");
    proposer->Stop();
    LogPrint(BCLog::FINALIZATION, "all proposer threads exited.\n");
  }
}

void WakeProposer(const CWallet *wallet) {
  if (proposer) {
    proposer->Wake(wallet);
  }
}

}  // namespace esperanza
