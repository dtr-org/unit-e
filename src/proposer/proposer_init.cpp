// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer_init.h>

#include <injector.h>
#include <util.h>

#include <mutex>
#include <utility>

namespace proposer {

static std::mutex initLock;
static std::unique_ptr<UnitEInjector> injector = nullptr;

bool InitProposer(const esperanza::Settings &settings,
                  const std::vector<CWallet *> &wallets) {
  std::unique_lock<decltype(initLock)> lock;
  if (injector) {
    return false;
  }
  if (!settings.m_proposing) {
    LogPrint(BCLog::FINALIZATION,
             "not starting proposer, proposing is not activated.\n");
    return true;
  }
  auto _injector = MakeUnique<UnitEInjector>();
  injector.swap(_injector);
  try {
    injector->Initialize();
  } catch (const std::runtime_error &exc) {
    LogPrint(BCLog::FINALIZATION, "failed to create proposer threads: %s\n",
             exc.what());
    return false;
  }
  return true;
}

void StartProposer() {
  if (injector) {
    LogPrint(BCLog::FINALIZATION, "starting proposer threads...\n");
    injector->GetProposer()->Start();
  }
}

void StopProposer() {
  if (injector) {
    LogPrint(BCLog::FINALIZATION, "stopping proposer threads...\n");
    injector->GetProposer()->Stop();
    LogPrint(BCLog::FINALIZATION, "all proposer threads exited.\n");
  }
}

void WakeProposer(const CWallet *wallet) {
  if (injector) {
    injector->GetProposer()->Wake(wallet);
  }
}

}  // namespace proposer
