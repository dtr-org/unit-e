#include <amount.h>
#include <consensus/validation.h>
#include <esperanza/finalizationparams.h>
#include <esperanza/finalization_utils.h>
#include <esperanza/vote.h>
#include <keystore.h>
#include <primitives/transaction.h>
#include <primitives/txtype.h>
#include <script/script.h>
#include <test/test_unite.h>
#include <txmempool.h>
#include <validation.h>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(walletextension_tests)

BOOST_FIXTURE_TEST_CASE(mempool_accept_deposit, TestChain100Setup) {

  CAmount amount = 1 * UNIT;
  CTransaction depositTx = CreateDeposit(coinbaseTxns[0], coinbaseKey, amount);

  CValidationState state;

  LOCK(cs_main);

  unsigned long initialPoolSize = mempool.size();

  BOOST_CHECK_EQUAL(
      true, AcceptToMemoryPool(mempool, state, MakeTransactionRef(depositTx),
                               nullptr /* pfMissingInputs */,
                               nullptr /* plTxnReplaced */,
                               true /* bypass_limits */, 0 /* nAbsurdFee */));

  BOOST_CHECK_EQUAL(mempool.size(), initialPoolSize + 1);
}

BOOST_FIXTURE_TEST_CASE(tx_mempool_accept_vote, TestChain100Setup) {
}

BOOST_AUTO_TEST_SUITE_END()
