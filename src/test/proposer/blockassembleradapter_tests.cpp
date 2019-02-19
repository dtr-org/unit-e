// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <test/test_unite.h>
#include <validation.h>

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <staking/transactionpicker.h>
#include <wallet/test/wallet_test_fixture.h>

struct RegtestingWalletSetup : public WalletTestingSetup {
  RegtestingWalletSetup() : WalletTestingSetup(CBaseChainParams::REGTEST) {}
};

BOOST_FIXTURE_TEST_SUITE(blockassembleradapter_tests, RegtestingWalletSetup)

BOOST_AUTO_TEST_CASE(block_assembler_adapter_test) {

  // this test checks the technical correctness, that is basically
  // that it does not crash and does yield a value. For a proper test
  // transactions will have to be mocked.

  auto blockAssemblerAdapter = staking::TransactionPicker::New();
  const auto params = staking::TransactionPicker::PickTransactionsParameters();

  auto result = blockAssemblerAdapter->PickTransactions(params);

  CAmount fees = 0;
  for (const auto fee : result.fees) {
    fees += fee;
  }

  // do something with the result to prevent it being optimized away
  BOOST_CHECK_GE(fees, 0);
}

BOOST_AUTO_TEST_CASE(pick_transactions_removes_bitcoin_coinbase) {

  const auto blockAssemblerAdapter = staking::TransactionPicker::New();
  const auto params = staking::TransactionPicker::PickTransactionsParameters();

  const auto result = blockAssemblerAdapter->PickTransactions(params);

  for (const auto &tx : result.transactions) {
    BOOST_CHECK(!tx->IsCoinBase());
  }
}

BOOST_AUTO_TEST_SUITE_END()
