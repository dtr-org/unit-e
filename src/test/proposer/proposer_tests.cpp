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

namespace proposer {

struct None {};

template <>
struct ProposerAccess<None> {
 private:
  const ProposerImpl &m_proposer;

 public:
  ProposerAccess(const ProposerImpl &proposer) : m_proposer(proposer) {}

  const std::vector<CWallet *> &wallets(const size_t ix) const {
    return m_proposer.m_threads[ix].m_wallets;
  }
};

typedef ProposerAccess<None> ProposerSpy;

}  // namespace proposer

struct WalletMock : public proposer::MultiWallet {
  std::vector<CWallet *> m_wallets;
  CWallet m_wallet;

  WalletMock() { m_wallets.emplace_back(&m_wallet); }

  const std::vector<CWallet *> &GetWallets() const { return m_wallets; }
};

BOOST_AUTO_TEST_SUITE(proposer_tests)

fakeit::Mock<staking::Network> networkMock;
fakeit::Mock<staking::ActiveChain> chainMock;

Dependency<staking::Network> network = &networkMock.get();
Dependency<staking::ActiveChain> chain = &chainMock.get();

BOOST_AUTO_TEST_CASE(start_stop) {
  Settings settings;

  WalletMock wallets;
  {
    proposer::ProposerImpl proposer(&settings, &wallets, network, chain);

    proposer.Start();
  }
  // UNIT-E: For now just checks that no exception has been thrown.
}

BOOST_AUTO_TEST_CASE(stop_without_start) {
  Settings settings;
  WalletMock wallets;
  {
    proposer::ProposerImpl proposer(&settings, &wallets, network, chain);
  }
  // UNIT-E: For now just checks that no exception has been thrown.
}

BOOST_AUTO_TEST_SUITE_END()
