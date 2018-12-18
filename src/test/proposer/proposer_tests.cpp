// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/multiwallet.h>
#include <proposer/proposer.h>
#include <test/test_unite.h>
#include <wallet/wallet.h>
#include <boost/test/unit_test.hpp>

#include <thread>

#include <test/fakeit/fakeit.hpp>

#if defined(__GNUG__) and not defined(__clang__)
// Fakeit does not work with GCC's devirtualization
// which is enabled with -O2 (the default) or higher.
#pragma GCC optimize("no-devirtualize")
#endif

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

  fakeit::Mock<staking::Network> network_mock;
  fakeit::Mock<staking::ActiveChain> chain_mock;
  fakeit::Mock<proposer::Logic> logic_mock;

  std::unique_ptr<Proposer> GetProposer() {
    return Proposer::New(
        settings.get(),
        behavior.get(),
        &multi_wallet_mock,
        &network_mock.get(),
        &chain_mock.get(),
        &logic_mock.get());
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
  fakeit::When(Method(f.network_mock, GetNodeCount)).Return(0);
  BOOST_CHECK_NO_THROW({
    auto p = f.GetProposer();
    p->Start();
  });
}

BOOST_AUTO_TEST_SUITE_END()
