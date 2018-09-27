// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer_init.h>

#include <proposer/blockproposer.h>
#include <proposer/chainstate.h>
#include <proposer/network.h>
#include <proposer/proposer.h>
#include <proposer/transactionpicker.h>
#include <util.h>

#include <mutex>
#include <utility>

namespace proposer {

static std::mutex initLock;
static std::unique_ptr<Proposer> proposer = nullptr;

bool InitProposer(const esperanza::Settings &settings,
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
  //  try {
  //    std::shared_ptr<ChainState> chainInterface(
  //        std::move(ChainState::MakeChain()));
  //    std::shared_ptr<Network> networkInterface(
  //        std::move(Network::MakeNetwork()));
  //    std::shared_ptr<TransactionPicker> transactionPicker(
  //        std::move(TransactionPicker::MakeBlockAssemblerAdapter(::Params())));
  //    std::shared_ptr<BlockProposer> blockProposer(std::move(
  //        BlockProposer::MakeBlockProposer(chainInterface,
  //        transactionPicker)));
  //    proposer.reset(new Proposer(settings, wallets, networkInterface,
  //                                chainInterface, blockProposer));
  //    return true;
  //  } catch (const std::runtime_error &exc) {
  //    LogPrint(BCLog::FINALIZATION, "failed to create proposer threads: %s\n",
  //             exc.what());
  //    return false;
  //  }
  return true;
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

}  // namespace proposer
