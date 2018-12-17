// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/multiwallet.h>
#include <proposer/proposer.h>
#include <test/test_unite.h>
#include <wallet/wallet.h>
#include <boost/test/unit_test.hpp>

#include <thread>

#if defined(__GNUG__) and not defined(__clang__)
// Fakeit does not work with GCC's devirtualization
// which is enabled with -O2 (the default) or higher.
#pragma GCC optimize("no-devirtualize")
#endif

#include <test/fakeit/fakeit.hpp>

namespace {

struct Fixture {

  using Proposer = proposer::Proposer;

  ArgsManager args_manager;

  Settings settings;
  blockchain::Parameters parameters = blockchain::Parameters::MainNet();
  std::unique_ptr<blockchain::Behavior> behavior = blockchain::Behavior::New(&args_manager);

  struct MultiWalletMock : public proposer::MultiWallet {
    std::vector<CWallet *> wallets;
    const std::vector<CWallet *> &GetWallets() const override {
      return wallets;
    };
  };

  CWallet wallet;
  MultiWalletMock multi_wallet_mock;

  Fixture()
      : wallet([&] {
          esperanza::WalletExtensionDeps deps;
          deps.settings = &settings;
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
        &settings,
        behavior.get(),
        &multi_wallet_mock,
        &network_mock.get(),
        &chain_mock.get(),
        &logic_mock.get());
  }
};

}  // namespace

BOOST_AUTO_TEST_SUITE(proposer_tests)

BOOST_FIXTURE_TEST_CASE(start_stop, Fixture) {
  BOOST_CHECK_NO_THROW([&] {
    auto p = GetProposer();
    p->Start();
  });
}

BOOST_AUTO_TEST_SUITE_END()
