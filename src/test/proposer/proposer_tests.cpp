// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer.h>

#include <blockchain/blockchain_behavior.h>
#include <proposer/block_builder.h>
#include <proposer/eligible_coin.h>
#include <proposer/multiwallet.h>
#include <proposer/proposer_logic.h>
#include <staking/active_chain.h>
#include <staking/network.h>
#include <staking/transactionpicker.h>
#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

#include <thread>

namespace {

struct Fixture {

  using Proposer = proposer::Proposer;

  std::unique_ptr<::ArgsManager> args_manager;
  std::unique_ptr<blockchain::Behavior> behavior = blockchain::Behavior::New(args_manager.get());
  std::unique_ptr<::Settings> settings;
  blockchain::Parameters parameters = blockchain::Parameters::TestNet();

  struct MultiWalletMock : public proposer::MultiWallet {
    std::vector<std::shared_ptr<CWallet>> wallets;
    const std::vector<std::shared_ptr<CWallet>> GetWallets() const override {
      return wallets;
    }
  };

  std::shared_ptr<CWallet> wallet;
  MultiWalletMock multi_wallet_mock;
  mocks::StakeValidatorMock stake_validator;

  Fixture(std::initializer_list<std::string> args)
      : args_manager([&] {
          std::unique_ptr<::ArgsManager> argsman = MakeUnique<::ArgsManager>();
          const char **argv = new const char *[args.size() + 1];
          argv[0] = "executable-name";
          std::size_t i = 1;
          for (const auto &arg : args) {
            argv[i++] = arg.c_str();
          }
          std::string error;
          argsman->ParseParameters(static_cast<int>(i), argv, error);
          delete[] argv;
          return argsman;
        }()),
        settings(Settings::New(args_manager.get(), behavior.get())),
        wallet(new CWallet("mock", WalletDatabase::CreateMock(), [&] {
          esperanza::WalletExtensionDeps deps(settings.get(), &stake_validator);
          return deps;
        }())),
        multi_wallet_mock([&] {
          MultiWalletMock mock;
          mock.wallets.emplace_back(wallet);
          return mock;
        }()) {}

  class NetworkMock : public staking::Network {
   public:
    size_t nodecount = 0;
    mutable size_t GetNodeCount_invocations = 0;
    int64_t GetTime() const override { return 0; }
    size_t GetNodeCount() const override {
      ++GetNodeCount_invocations;
      return nodecount;
    }
    size_t GetInboundNodeCount() const override { return nodecount; }
    size_t GetOutboundNodeCount() const override { return 0; }
  };

  class ActiveChainMock : public staking::ActiveChain {
   public:
    ::SyncStatus syncstatus = ::SyncStatus::IMPORTING;
    mutable size_t GetInitialBlockDownloadStatus_invocations = 0;
    mutable CCriticalSection m_lock;
    CCriticalSection &GetLock() const override { return m_lock; }
    blockchain::Height GetSize() const override { return 1; }
    blockchain::Height GetHeight() const override { return 0; }
    const CBlockIndex *GetTip() const override { return nullptr; }
    const CBlockIndex *GetGenesis() const override { return nullptr; }
    bool Contains(const CBlockIndex &) const override { return false; }
    const CBlockIndex *FindForkOrigin(const CBlockIndex &) const override { return nullptr; }
    const CBlockIndex *GetNext(const CBlockIndex &) const override { return nullptr; }
    const CBlockIndex *AtDepth(blockchain::Depth depth) const override { return nullptr; }
    const CBlockIndex *AtHeight(blockchain::Height height) const override { return nullptr; }
    blockchain::Depth GetDepth(const blockchain::Height) const override { return 0; }
    const CBlockIndex *GetBlockIndex(const uint256 &) const override { return nullptr; }
    const uint256 ComputeSnapshotHash() const override { return uint256(); }
    bool ProposeBlock(std::shared_ptr<const CBlock> pblock) override { return false; }
    ::SyncStatus GetInitialBlockDownloadStatus() const override {
      ++GetInitialBlockDownloadStatus_invocations;
      return syncstatus;
    }
    boost::optional<staking::Coin> GetUTXO(const COutPoint &) const override { return boost::none; };
  };

  class TransactionPickerMock : public staking::TransactionPicker {
    PickTransactionsResult PickTransactions(const PickTransactionsParameters &) override {
      return PickTransactionsResult();
    }
  };

  class BlockBuilderMock : public proposer::BlockBuilder {
    const CTransactionRef BuildCoinbaseTransaction(
        const uint256 &,
        const proposer::EligibleCoin &,
        const staking::CoinSet &,
        CAmount,
        const boost::optional<CScript> &coinbase_script,
        staking::StakingWallet &) const override { return nullptr; };
    std::shared_ptr<const CBlock> BuildBlock(
        const CBlockIndex &,
        const uint256 &,
        const proposer::EligibleCoin &,
        const staking::CoinSet &,
        const std::vector<CTransactionRef> &,
        const CAmount,
        const boost::optional<CScript> &coinbase_script,
        staking::StakingWallet &) const override { return nullptr; }
  };

  class ProposerLogicMock : public proposer::Logic {
   public:
    boost::optional<proposer::EligibleCoin> TryPropose(const staking::CoinSet &) override { return boost::none; }
  };

  NetworkMock network_mock;
  ActiveChainMock chain_mock;
  TransactionPickerMock transaction_picker_mock;
  BlockBuilderMock block_builder_mock;
  ProposerLogicMock logic_mock;

  std::unique_ptr<Proposer> MakeProposer() {
    return Proposer::New(
        settings.get(),
        behavior.get(),
        &multi_wallet_mock,
        &network_mock,
        &chain_mock,
        &transaction_picker_mock,
        &block_builder_mock,
        &logic_mock);
  }
};

}  // namespace

BOOST_AUTO_TEST_SUITE(proposer_tests)

BOOST_AUTO_TEST_CASE(not_proposing_stub_vs_actual_impl) {
  Fixture f1{"-proposing=0"};
  Fixture f2{"-proposing=0"};
  Fixture f3{"-proposing=1"};
  Fixture f4{"-proposing=1"};

  std::unique_ptr<proposer::Proposer> p1 = f1.MakeProposer();
  std::unique_ptr<proposer::Proposer> p2 = f2.MakeProposer();
  std::unique_ptr<proposer::Proposer> p3 = f3.MakeProposer();
  std::unique_ptr<proposer::Proposer> p4 = f4.MakeProposer();

  proposer::Proposer &r1 = *p1;
  proposer::Proposer &r2 = *p2;
  proposer::Proposer &r3 = *p3;
  proposer::Proposer &r4 = *p4;

  BOOST_CHECK(typeid(r1) == typeid(r2));
  BOOST_CHECK(typeid(r3) == typeid(r4));
  BOOST_CHECK(typeid(r1) != typeid(r3));
  BOOST_CHECK(typeid(r2) != typeid(r4));
}

BOOST_AUTO_TEST_CASE(start_stop_and_status) {
  Fixture f{"-proposing=1"};
  f.network_mock.nodecount = 0;
  BOOST_CHECK_NO_THROW({
    auto p = f.MakeProposer();
    p->Start();
  });
  // destroying the proposer stops it
  BOOST_CHECK(f.network_mock.GetNodeCount_invocations > 0);
  BOOST_CHECK_EQUAL(f.wallet->GetWalletExtension().GetProposerState().m_status, +proposer::Status::NOT_PROPOSING_NO_PEERS);
}

BOOST_AUTO_TEST_CASE(advance_to_blockchain_sync) {
  Fixture f{"-proposing=1"};
  f.network_mock.nodecount = 1;
  BOOST_CHECK_NO_THROW({
    auto p = f.MakeProposer();
    p->Start();
  });
  BOOST_CHECK(f.chain_mock.GetInitialBlockDownloadStatus_invocations > 0);
  BOOST_CHECK_EQUAL(f.wallet->GetWalletExtension().GetProposerState().m_status, +proposer::Status::NOT_PROPOSING_SYNCING_BLOCKCHAIN);
}

BOOST_AUTO_TEST_SUITE_END()
