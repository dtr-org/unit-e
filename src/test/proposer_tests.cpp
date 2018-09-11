// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/proposer.h>
#include <test/test_unite.h>
#include <wallet/wallet.h>
#include <boost/test/unit_test.hpp>

#include <thread>

namespace esperanza {

struct None {};

template <>
struct ProposerAccess<None> {
 private:
  const Proposer& m_proposer;

 public:
  ProposerAccess(const Proposer& proposer) : m_proposer(proposer) {}

  size_t numThreads() const { return m_proposer.m_threads.size(); }

  const std::vector<CWallet*>& wallets(const size_t ix) const {
    return m_proposer.m_threads[ix]->m_wallets;
  }
};

typedef ProposerAccess<None> ProposerSpy;

}  // namespace esperanza

BOOST_AUTO_TEST_SUITE(proposer_tests)

BOOST_AUTO_TEST_CASE(start_stop) {
  esperanza::Config config;

  config.m_numberOfProposerThreads = 0;

  std::vector<CWallet*> wallets;
  CWallet wallet;
  wallets.emplace_back(&wallet);

  esperanza::Proposer proposer(config, wallets);

  proposer.Start();
  proposer.Stop();
}

BOOST_AUTO_TEST_CASE(stop_twice) {
  esperanza::Config config;
  std::vector<CWallet*> wallets;
  CWallet wallet;
  wallets.emplace_back(&wallet);

  esperanza::Proposer proposer(config, wallets);

  proposer.Start();
  proposer.Stop();
  proposer.Stop();
}

BOOST_AUTO_TEST_CASE(stop_without_start) {
  esperanza::Config config;
  std::vector<CWallet*> wallets;
  CWallet wallet;
  wallets.emplace_back(&wallet);

  esperanza::Proposer proposer(config, wallets);

  proposer.Stop();
}

BOOST_AUTO_TEST_CASE(stop_twice_without_start) {
  esperanza::Config config;
  std::vector<CWallet*> wallets;
  CWallet wallet;
  wallets.emplace_back(&wallet);

  esperanza::Proposer proposer(config, wallets);

  proposer.Stop();
  proposer.Stop();
}

BOOST_AUTO_TEST_CASE(wallet_distribution) {
  esperanza::Config config;

  config.m_numberOfProposerThreads = 3;

  std::vector<CWallet*> wallets;

  CWallet w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11;

  wallets.emplace_back(&w1);
  wallets.emplace_back(&w2);
  wallets.emplace_back(&w3);
  wallets.emplace_back(&w4);
  wallets.emplace_back(&w5);
  wallets.emplace_back(&w6);
  wallets.emplace_back(&w7);
  wallets.emplace_back(&w8);
  wallets.emplace_back(&w9);
  wallets.emplace_back(&w10);
  wallets.emplace_back(&w11);

  esperanza::Proposer proposer(config, wallets);
  esperanza::ProposerSpy spy(proposer);

  std::cout << spy.numThreads() << std::endl;
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
  esperanza::Config config;

  config.m_numberOfProposerThreads = 17;

  std::vector<CWallet*> wallets;
  CWallet wallet;
  wallets.emplace_back(&wallet);

  esperanza::Proposer proposer(config, wallets);
  esperanza::ProposerSpy spy(proposer);

  BOOST_CHECK(spy.numThreads() == 1);
  BOOST_CHECK(spy.wallets(0).size() == 1);
  BOOST_CHECK(spy.wallets(0)[0] == &wallet);
}

BOOST_AUTO_TEST_CASE(single_wallet_too_few_threads_specified) {
  esperanza::Config config;

  config.m_numberOfProposerThreads = 0;

  std::vector<CWallet*> wallets;
  CWallet wallet;
  wallets.emplace_back(&wallet);

  esperanza::Proposer proposer(config, wallets);
  esperanza::ProposerSpy spy(proposer);

  BOOST_CHECK(spy.numThreads() == 1);
  BOOST_CHECK(spy.wallets(0).size() == 1);
  BOOST_CHECK(spy.wallets(0)[0] == &wallet);
}

BOOST_AUTO_TEST_SUITE_END()
