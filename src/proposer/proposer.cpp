// Copyright (c) 2018-2019 The Unit-e developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer.h>

#include <chainparams.h>
#include <net.h>
#include <proposer/block_builder.h>
#include <proposer/multiwallet.h>
#include <proposer/proposer_logic.h>
#include <proposer/proposer_status.h>
#include <proposer/sync.h>
#include <proposer/waiter.h>
#include <script/script.h>
#include <staking/active_chain.h>
#include <staking/network.h>
#include <staking/transactionpicker.h>
#include <sync.h>
#include <util.h>
#include <utilmoneystr.h>
#include <wallet/wallet.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <numeric>

namespace proposer {

class ProposerStub : public Proposer {
 public:
  void Wake() override {}
  void Start() override {}
  void Stop() override {}
  bool IsStarted() override { return false; }
};

class ProposerImpl : public Proposer {
 private:
  static constexpr const char *THREAD_NAME = "unite-proposer";

  const Dependency<blockchain::Behavior> m_blockchain_behavior;
  const Dependency<MultiWallet> m_multi_wallet;
  const Dependency<staking::Network> m_network;
  const Dependency<staking::ActiveChain> m_active_chain;
  const Dependency<staking::TransactionPicker> m_transaction_picker;
  const Dependency<proposer::BlockBuilder> m_block_builder;
  const Dependency<proposer::Logic> m_proposer_logic;

  std::thread m_thread;
  enum {
    INITIALIZED,
    STARTED,
    STOPPED
  } m_state = INITIALIZED;
  std::atomic_bool m_interrupted;
  Waiter m_waiter;

  void SetStatusOfAllWallets(const Status &status) {
    for (const auto &wallet : m_multi_wallet->GetWallets()) {
      wallet->GetWalletExtension().GetProposerState().m_status = status;
    }
  }

  bool Wait() {
    // UNIT-E simply wait. the proposer could be woken up when a new block arrives.
    if (m_interrupted) {
      return false;
    }
    m_waiter.WaitUpTo(m_blockchain_behavior->GetBlockStakeTimestampInterval());
    return !m_interrupted;
  }

  void Run() {
    RenameThread(THREAD_NAME);
    LogPrint(BCLog::PROPOSING, "Proposer thread started.\n");
    do {
      if (m_network->GetNodeCount() == 0) {
        LogPrint(BCLog::PROPOSING, "Not proposing, no peers\n");
        SetStatusOfAllWallets(Status::NOT_PROPOSING_NO_PEERS);
        continue;
      }
      if (m_active_chain->GetInitialBlockDownloadStatus() != +::SyncStatus::SYNCED) {
        LogPrint(BCLog::PROPOSING, "Not proposing, syncing blockchain\n");
        SetStatusOfAllWallets(Status::NOT_PROPOSING_SYNCING_BLOCKCHAIN);
        continue;
      }
      for (CWalletRef wallet : m_multi_wallet->GetWallets()) {
        auto &wallet_ext = wallet->GetWalletExtension();
        const auto wallet_name = wallet->GetName();
        if (wallet->IsLocked()) {
          LogPrint(BCLog::PROPOSING, "Not proposing, wallet locked (wallet=%s)\n", wallet_name);
          wallet_ext.GetProposerState().m_status = Status::NOT_PROPOSING_WALLET_LOCKED;
          continue;
        }
        if (m_interrupted) {
          break;
        }
        std::shared_ptr<const CBlock> block;
        {
          LOCK2(m_active_chain->GetLock(), wallet_ext.GetLock());
          const CBlockIndex &tip = *m_active_chain->GetTip();
          const staking::CoinSet coins = wallet_ext.GetStakeableCoins();
          if (coins.empty()) {
            LogPrint(BCLog::PROPOSING, "Not proposing, not enough balance (wallet=%s)\n", wallet_name);
            wallet_ext.GetProposerState().m_status = Status::NOT_PROPOSING_NOT_ENOUGH_BALANCE;
            continue;
          }
          wallet_ext.GetProposerState().m_status = Status::IS_PROPOSING;
          const boost::optional<EligibleCoin> &winning_ticket = m_proposer_logic->TryPropose(coins);
          if (!winning_ticket) {
            LogPrint(BCLog::PROPOSING, "Not proposing this time (wallet=%s)\n", wallet_name);
            continue;
          }
          const EligibleCoin &coin = winning_ticket.get();
          LogPrint(BCLog::PROPOSING, "Proposing... (wallet=%s, coin=%s)\n",
                   wallet_name, util::to_string(coin.utxo));
          staking::TransactionPicker::PickTransactionsParameters parameters{};
          staking::TransactionPicker::PickTransactionsResult result =
              m_transaction_picker->PickTransactions(parameters);

          if (!result) {
            LogPrint(BCLog::PROPOSING, "Failed to pick transactions (wallet=%s, error=%s) – proposing empty block.\n", wallet_name, result.error);
          }
          const CAmount fees = std::accumulate(result.fees.begin(), result.fees.end(), CAmount(0));
          const uint256 snapshot_hash = m_active_chain->ComputeSnapshotHash();

          block = m_block_builder->BuildBlock(
              tip, snapshot_hash, coin, coins, result.transactions, fees, wallet_ext);
        }
        if (m_interrupted) {
          break;
        }
        if (!block) {
          LogPrint(BCLog::PROPOSING, "Failed to assemble block.\n");
          continue;
        }
        const auto &hash = block->GetHash().GetHex();
        if (!m_active_chain->ProposeBlock(block)) {
          LogPrint(BCLog::PROPOSING, "Failed to propose block (hash=%s).\n", hash);
          continue;
        }
        LogPrint(BCLog::PROPOSING, "Proposed new block (hash=%s).\n", hash);
      }
    } while (Wait());
    LogPrint(BCLog::PROPOSING, "Proposer thread stopping...\n");
  }

 public:
  ProposerImpl(const Dependency<blockchain::Behavior> blockchain_behavior,
               const Dependency<MultiWallet> multi_wallet,
               const Dependency<staking::Network> network,
               const Dependency<staking::ActiveChain> active_chain,
               const Dependency<staking::TransactionPicker> transaction_picker,
               const Dependency<proposer::BlockBuilder> block_builder,
               const Dependency<proposer::Logic> proposer_logic)
      : m_blockchain_behavior(blockchain_behavior),
        m_multi_wallet(multi_wallet),
        m_network(network),
        m_active_chain(active_chain),
        m_transaction_picker(transaction_picker),
        m_block_builder(block_builder),
        m_proposer_logic(proposer_logic),
        m_interrupted(false) {
  }

  void Wake() override {
    m_waiter.Wake();
  }

  void Start() override {
    if (m_state != INITIALIZED) {
      LogPrint(BCLog::PROPOSING, "Proposer already started, not starting again.\n");
      return;
    }
    m_thread = std::thread(&ProposerImpl::Run, this);
    m_state = STARTED;
  }

  void Stop() override {
    if (m_state != STARTED) {
      LogPrint(BCLog::PROPOSING, "Proposer not started, nothing to stop.\n");
      return;
    }
    LogPrint(BCLog::PROPOSING, "Stopping proposer thread...\n");
    m_interrupted = true;
    Wake();
    m_thread.join();
    m_state = STOPPED;
    LogPrint(BCLog::PROPOSING, "Proposer stopped.\n");
  }

  bool IsStarted() override {
    return m_state == STARTED;
  }

  ~ProposerImpl() override {
    Stop();
  };
};

std::unique_ptr<Proposer> Proposer::New(
    const Dependency<Settings> settings,
    const Dependency<blockchain::Behavior> behavior,
    const Dependency<MultiWallet> multi_wallet,
    const Dependency<staking::Network> network,
    const Dependency<staking::ActiveChain> active_chain,
    const Dependency<staking::TransactionPicker> transaction_picker,
    const Dependency<BlockBuilder> block_builder,
    const Dependency<Logic> proposer_logic) {
  if (settings->node_is_proposer) {
    return std::unique_ptr<Proposer>(new ProposerImpl(behavior, multi_wallet, network, active_chain, transaction_picker, block_builder, proposer_logic));
  } else {
    return std::unique_ptr<Proposer>(new ProposerStub());
  }
}

}  // namespace proposer
