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
  const Proposer &m_proposer;

 public:
  ProposerAccess(const Proposer &proposer) : m_proposer(proposer) {}

  size_t numThreads() const { return m_proposer.m_threads.size(); }

  const std::vector<CWallet *> &wallets(const size_t ix) const {
    return m_proposer.m_threads[ix]->m_wallets;
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

fakeit::Mock<proposer::Network> networkMock;
fakeit::Mock<proposer::ChainState> chainMock;
fakeit::Mock<proposer::BlockProposer> blockProposerMock;

Dependency<proposer::Network> network = &networkMock.get();
Dependency<proposer::ChainState> chain = &chainMock.get();
Dependency<proposer::BlockProposer> blockProposer = &blockProposerMock.get();

BOOST_AUTO_TEST_CASE(start_stop) {
  proposer::Settings config;

  config.m_numberOfProposerThreads = 0;

  WalletMock wallets;

  proposer::Proposer proposer(&config, &wallets, network, chain, blockProposer);

  proposer.Start();
  proposer.Stop();
}

BOOST_AUTO_TEST_CASE(stop_twice) {
  proposer::Settings config;
  WalletMock wallets;

  proposer::Proposer proposer(&config, &wallets, network, chain, blockProposer);

  proposer.Start();
  proposer.Stop();
  proposer.Stop();
}

BOOST_AUTO_TEST_CASE(stop_without_start) {
  proposer::Settings config;
  WalletMock wallets;

  proposer::Proposer proposer(&config, &wallets, network, chain, blockProposer);

  proposer.Stop();
}

BOOST_AUTO_TEST_CASE(stop_twice_without_start) {
  proposer::Settings config;
  WalletMock wallets;

  proposer::Proposer proposer(&config, &wallets, network, chain, blockProposer);

  proposer.Stop();
  proposer.Stop();
}

BOOST_AUTO_TEST_CASE(wallet_distribution) {
  proposer::Settings config;

  config.m_numberOfProposerThreads = 3;

  WalletMock wallets;
  wallets.m_wallets.clear();

  CWallet w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11;

  wallets.m_wallets.emplace_back(&w1);
  wallets.m_wallets.emplace_back(&w2);
  wallets.m_wallets.emplace_back(&w3);
  wallets.m_wallets.emplace_back(&w4);
  wallets.m_wallets.emplace_back(&w5);
  wallets.m_wallets.emplace_back(&w6);
  wallets.m_wallets.emplace_back(&w7);
  wallets.m_wallets.emplace_back(&w8);
  wallets.m_wallets.emplace_back(&w9);
  wallets.m_wallets.emplace_back(&w10);
  wallets.m_wallets.emplace_back(&w11);

  proposer::Proposer proposer(&config, &wallets, network, chain, blockProposer);
  proposer::ProposerSpy spy(proposer);

  BOOST_CHECK(spy.numThreads() == 3);
  BOOST_CHECK(spy.wallets(0).size() == 4);
  BOOST_CHECK(spy.wallets(1).size() == 4);
  BOOST_CHECK(spy.wallets(2).size() == 3);

  BOOST_CHECK(spy.wallets(0)[0] == &w1);
  BOOST_CHECK(spy.wallets(1)[0] == &w2);
  BOOST_CHECK(spy.wallets(2)[0] == &w3);
  BOOST_CHECK(spy.wallets(0)[1] == &w4);
  BOOST_CHECK(spy.wallets(1)[1] == &w5);
  BOOST_CHECK(spy.wallets(2)[1] == &w6);
  BOOST_CHECK(spy.wallets(0)[2] == &w7);
  BOOST_CHECK(spy.wallets(1)[2] == &w8);
  BOOST_CHECK(spy.wallets(2)[2] == &w9);
  BOOST_CHECK(spy.wallets(0)[3] == &w10);
  BOOST_CHECK(spy.wallets(1)[3] == &w11);
}

BOOST_AUTO_TEST_CASE(single_wallet_too_many_threads_specified) {
  proposer::Settings config;

  config.m_numberOfProposerThreads = 17;

  WalletMock wallets;

  proposer::Proposer proposer(&config, &wallets, network, chain, blockProposer);
  proposer::ProposerSpy spy(proposer);

  BOOST_CHECK(spy.numThreads() == 1);
  BOOST_CHECK(spy.wallets(0).size() == 1);
  BOOST_CHECK(spy.wallets(0)[0] == &wallets.m_wallet);
}

BOOST_AUTO_TEST_CASE(single_wallet_too_few_threads_specified) {
  proposer::Settings config;

  config.m_numberOfProposerThreads = 0;

  WalletMock wallets;

  proposer::Proposer proposer(&config, &wallets, network, chain, blockProposer);
  proposer::ProposerSpy spy(proposer);

  BOOST_CHECK(spy.numThreads() == 1);
  BOOST_CHECK(spy.wallets(0).size() == 1);
  BOOST_CHECK(spy.wallets(0)[0] == &wallets.m_wallet);
}

BOOST_AUTO_TEST_SUITE_END()
