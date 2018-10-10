// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <test/test_unite.h>
#include <validation.h>

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <proposer/transactionpicker.h>

struct RegtestingSetup : public TestingSetup {
  RegtestingSetup() : TestingSetup(CBaseChainParams::REGTEST) {}
};

BOOST_FIXTURE_TEST_SUITE(blockassembleradapter_tests, RegtestingSetup)

BOOST_AUTO_TEST_CASE(block_assembler_adapter_test) {

  // this test checks the technical correctness, that is basically
  // that it does not crash and does yield a value. For a proper test
  // transactions will have to be mocked.

  std::unique_ptr<proposer::TransactionPicker> blockAssemblerAdapter =
      proposer::TransactionPicker::BlockAssemblerAdapter(::Params());

  proposer::TransactionPicker::PickTransactionsParameters params;

  auto result = blockAssemblerAdapter->PickTransactions(params);

  CAmount fees = 0;
  for (const auto fee : result.m_fees) {
    fees += fee;
  }

  // do something with the result to prevent it being optimized away
  BOOST_CHECK_GE(fees, 0);
}

BOOST_AUTO_TEST_SUITE_END()
