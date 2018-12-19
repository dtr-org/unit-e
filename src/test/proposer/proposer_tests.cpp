// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/multiwallet.h>
#include <proposer/proposer.h>
#include <test/test_unite.h>
#include <wallet/wallet.h>
#include <boost/test/unit_test.hpp>

#include <thread>

namespace {

struct Fixture {

  using Proposer = proposer::Proposer;

  std::unique_ptr<::ArgsManager> args_manager;
  std::unique_ptr<::Settings> settings;
  blockchain::Parameters parameters = blockchain::Parameters::MainNet();
  std::unique_ptr<blockchain::Behavior> behavior = blockchain::Behavior::New(args_manager.get());

  struct MultiWalletMock : public proposer::MultiWallet {
    std::vector<CWallet *> wallets;
    const std::vector<CWallet *> &GetWallets() const override {
      return wallets;
    };
  };

  CWallet wallet;
  MultiWalletMock multi_wallet_mock;

  Fixture(std::initializer_list<std::string> args)
      : args_manager([&] {
          std::unique_ptr<::ArgsManager> argsman = MakeUnique<::ArgsManager>();
          const char **argv = new const char *[args.size() + 1];
          argv[0] = "executable-name";
          std::size_t i = 1;
          for (const auto &arg : args) {
            argv[i++] = arg.c_str();
          }
          argsman->ParseParameters(static_cast<int>(i), argv);
          delete[] argv;
          return argsman;
        }()),
        settings(Settings::New(args_manager.get())),
        wallet([&] {
          esperanza::WalletExtensionDeps deps;
          deps.settings = settings.get();
          return deps;
        }()),
        multi_wallet_mock([&] {
          MultiWalletMock mock;
          mock.wallets.emplace_back(&wallet);
          return mock;
        }()) {}

  class NetworkMock : public staking::Network {
   public:
    size_t nodecount = 0;
    int64_t GetTime() const { return 0; };
    size_t GetNodeCount() { return nodecount; };
    size_t GetInboundNodeCount() { return nodecount; };
    size_t GetOutboundNodeCount() { return 0; };
  };

  class ActiveChainMock : public staking::ActiveChain {
   public:
    CCriticalSection m_lock;
    CCriticalSection &GetLock() override { return m_lock; }
    blockchain::Height GetSize() const override { return 1; }
    blockchain::Height GetHeight() const override { return 0; }
    const CBlockIndex *operator[](std::int64_t) override { return nullptr; };
    const CBlockIndex *AtDepth(blockchain::Depth depth) override { return nullptr; }
    const CBlockIndex *AtHeight(blockchain::Height height) override { return nullptr; }
    virtual bool ProcessNewBlock(std::shared_ptr<const CBlock> pblock) override { return false; };
    virtual ::SyncStatus GetInitialBlockDownloadStatus() const override { return ::SyncStatus::IMPORTING; };
  };

  class ProposerLogicMock : public proposer::Logic {
   public:
    boost::optional<COutput> TryPropose(const std::vector<COutput> &) override { return boost::none; };
  };

  NetworkMock network_mock;
  ActiveChainMock chain_mock;
  ProposerLogicMock logic_mock;

  std::unique_ptr<Proposer> GetProposer() {
    return Proposer::New(
        settings.get(),
        behavior.get(),
        &multi_wallet_mock,
        &network_mock,
        &chain_mock,
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

  std::unique_ptr<proposer::Proposer> p1 = f1.GetProposer();
  std::unique_ptr<proposer::Proposer> p2 = f2.GetProposer();
  std::unique_ptr<proposer::Proposer> p3 = f3.GetProposer();
  std::unique_ptr<proposer::Proposer> p4 = f4.GetProposer();

  proposer::Proposer &r1 = *p1;
  proposer::Proposer &r2 = *p2;
  proposer::Proposer &r3 = *p3;
  proposer::Proposer &r4 = *p4;

  BOOST_CHECK(typeid(r1) == typeid(r2));
  BOOST_CHECK(typeid(r3) == typeid(r4));
  BOOST_CHECK(typeid(r1) != typeid(r3));
  BOOST_CHECK(typeid(r2) != typeid(r4));
}

BOOST_AUTO_TEST_CASE(start_stop) {
  Fixture f{"-proposing=1"};
  f.network_mock.nodecount = 1;
  BOOST_CHECK_NO_THROW({
    auto p = f.GetProposer();
    p->Start();
  });
}

BOOST_AUTO_TEST_SUITE_END()
