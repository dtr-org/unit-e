// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <test/test_unite.h>

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <proposer/transactionpicker.h>

BOOST_FIXTURE_TEST_SUITE(blockassembleradapter_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(block_assembler_adapter_test) {

  std::unique_ptr<proposer::TransactionPicker> blockAssemblerAdapter =
      proposer::TransactionPicker::BlockAssemblerAdapter(::Params());

  proposer::TransactionPicker::PickTransactionsParameters params;

  auto result = blockAssemblerAdapter->PickTransactions(params);

  CAmount fees = 0;
  for (const auto fee : result.m_fees) {
    fees += fee;
  }
  BOOST_CHECK_GE(fees, 0);
}

BOOST_AUTO_TEST_SUITE_END()
