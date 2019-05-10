// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_behavior.h>
#include <consensus/validation.h>
#include <keystore.h>
#include <script/sign.h>

#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
#include <test/util/txtools.h>
#include <boost/test/unit_test.hpp>

namespace {

template <class T, std::size_t N>
constexpr std::size_t size(const T (&array)[N]) noexcept {
  return N;
}

}  // namespace

BOOST_FIXTURE_TEST_SUITE(blockchain_behavior_test, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(creation_test) {
  ArgsManager args;
  std::string error;

  {
    const char *const params[] = {"unit-e"};
    args.ParseParameters(size(params), params, error);
    const auto b = blockchain::Behavior::New(&args);
    BOOST_CHECK_EQUAL(b->GetNetworkName(), "test");
  }

  {
    const char *const params[] = {"unit-e", "-regtest"};
    args.ParseParameters(size(params), params, error);
    const auto b = blockchain::Behavior::New(&args);
    BOOST_CHECK_EQUAL(b->GetNetworkName(), "regtest");
  }

  {
    const char *const params[] = {"unit-e", "-regtest=0"};
    args.ParseParameters(size(params), params, error);
    const auto b = blockchain::Behavior::New(&args);
    BOOST_CHECK_EQUAL(b->GetNetworkName(), "test");
  }
}

BOOST_AUTO_TEST_CASE(GetTransactionWeight_test) {
  mocks::ArgsManagerMock args{};
  const auto b = blockchain::Behavior::New(&args);

  {
    // Check weight of empty transaction
    CTransaction tx;

    const std::size_t expected = GetTransactionWeight(tx);
    const std::size_t weight = b->GetTransactionWeight(tx);

    BOOST_CHECK_EQUAL(40, weight);
    BOOST_CHECK_EQUAL(expected, weight);
  }

  txtools::TxTool key_tool;
  {
    // Check weight of standard transaction
    const CTransaction tx = key_tool.CreateTransaction();

    const std::size_t expected = GetTransactionWeight(tx);
    const std::size_t weight = b->GetTransactionWeight(tx);

    BOOST_CHECK_EQUAL(437, weight);
    BOOST_CHECK_EQUAL(expected, weight);
  }
}

BOOST_AUTO_TEST_CASE(GetBlockWeight_test) {
  mocks::ArgsManagerMock args{};
  const auto b = blockchain::Behavior::New(&args);

  {
    // Check weight of empty block
    CBlock block;

    const std::size_t expected = GetBlockWeight(block);
    const std::size_t weight = b->GetBlockWeight(block);

    BOOST_CHECK_EQUAL(568, weight);
    BOOST_CHECK_EQUAL(expected, weight);
  }

  txtools::TxTool key_tool;
  {
    // Check weight of some block
    CBlock block;
    const CTransaction tx = key_tool.CreateTransaction();
    block.vtx.emplace_back(MakeTransactionRef(tx));

    const std::size_t expected = GetBlockWeight(block);
    const std::size_t weight = b->GetBlockWeight(block);

    BOOST_CHECK_GE(weight, 1004);
    BOOST_CHECK_LE(weight, 1005);
    BOOST_CHECK_EQUAL(expected, weight);
  }
}

BOOST_AUTO_TEST_CASE(GetTransactionInputWeight_test) {
  mocks::ArgsManagerMock args{};
  const auto b = blockchain::Behavior::New(&args);

  {
    // Check weight of empty transaction input
    CTxIn txin;

    const std::size_t expected = GetTransactionInputWeight(txin);
    const std::size_t weight = b->GetTransactionInputWeight(txin);

    BOOST_CHECK_EQUAL(165, weight);
    BOOST_CHECK_EQUAL(expected, weight);
  }

  txtools::TxTool key_tool;
  {
    // Check weight of minimal transaction input
    const CTransaction tx = key_tool.CreateTransaction();
    const CTxIn txin = tx.vin[0];
    const std::size_t expected = GetTransactionInputWeight(txin);
    const std::size_t weight = b->GetTransactionInputWeight(txin);

    BOOST_CHECK_GE(weight, 270);
    BOOST_CHECK_LE(weight, 271);
    BOOST_CHECK_EQUAL(expected, weight);
  }
}

BOOST_AUTO_TEST_SUITE_END()
