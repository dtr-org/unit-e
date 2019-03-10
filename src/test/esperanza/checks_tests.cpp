// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/checks.h>
#include <esperanza/finalizationstate.h>
#include <injector.h>
#include <keystore.h>
#include <random.h>
#include <script/script.h>
#include <test/esperanza/finalization_utils.h>
#include <test/esperanza/finalizationstate_utils.h>
#include <test/test_unite.h>
#include <util.h>
#include <validation.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

using namespace esperanza;

BOOST_FIXTURE_TEST_SUITE(finalization_checks_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(IsVoteExpired_test) {
  // This test changes tip's finalization state and inderectly checks it via IsVoteExpired().

  FinalizationState *esperanza = GetComponent<finalization::StateRepository>()->GetTipState();
  assert(esperanza != nullptr);

  const auto &params = CreateChainParams(CBaseChainParams::TESTNET)->GetFinalization();
  const auto min_deposit = params.min_deposit_size;
  const auto epoch_length = params.epoch_length;

  CKey k;
  InsecureNewKey(k, true);
  uint160 validatorAddress = k.GetPubKey().GetID();

  BOOST_CHECK_EQUAL(
      esperanza->ValidateDeposit(validatorAddress, min_deposit),
      +Result::SUCCESS);

  esperanza->ProcessDeposit(validatorAddress, min_deposit);

  // Initialize few epoch - since epoch 4 we don't have instant finalization
  for (int i = 1; i < 6; ++i) {
    BOOST_CHECK_EQUAL(esperanza->InitializeEpoch(i * epoch_length),
                      +Result::SUCCESS);
  }

  uint256 targetHash = uint256();

  Vote expired{RandValidatorAddr(), targetHash, 0, 2};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(expired, k)), true);

  Vote current{RandValidatorAddr(), targetHash, 0, 5};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(current, k)), false);

  Vote afterLastFinalization{RandValidatorAddr(), targetHash, 0, 3};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(afterLastFinalization, k)), true);

  Vote future{RandValidatorAddr(), targetHash, 0, 12};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(future, k)), false);

  Vote currentOtherFork{RandValidatorAddr(), GetRandHash(), 0, 5};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(currentOtherFork, k)), false);
}

BOOST_AUTO_TEST_CASE(CheckVoteTransaction_malformed_vote) {

  CKey key;
  key.MakeNewKey(true);
  Vote vote = Vote{key.GetPubKey().GetID(), GetRandHash(), 0, 2};
  CTransaction tx = CreateVoteTx(vote, key);
  CMutableTransaction mutedTx(tx);

  // Replace the vote with something meaningless
  mutedTx.vin[0].scriptSig = CScript() << 1337;

  CTransaction invalidVote(mutedTx);
  Consensus::Params params = Params().GetConsensus();
  CValidationState err_state;
  BOOST_CHECK(CheckFinalizationTx(invalidVote, err_state) == false);

  const FinalizationState *fin_state = GetComponent<finalization::StateRepository>()->GetTipState();
  assert(fin_state != nullptr);

  BOOST_CHECK(ContextualCheckVoteTx(invalidVote, err_state, params, *fin_state) == false);

  BOOST_CHECK_EQUAL("bad-vote-data-format", err_state.GetRejectReason());
}

BOOST_AUTO_TEST_CASE(ExtractValidatorIndex_deposit) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::DEPOSIT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction deposit = CreateDepositTx(prevTx, k, 10000);
  uint160 validatorAddress = uint160();
  BOOST_CHECK(ExtractValidatorAddress(deposit, validatorAddress));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetID().GetHex(), validatorAddress.GetHex());
}

BOOST_AUTO_TEST_CASE(contextual_check_deposit_tx) {
  CKey key;
  InsecureNewKey(key, true);

  CMutableTransaction mtx;
  mtx.SetType(TxType::DEPOSIT);
  mtx.vin.resize(1);
  mtx.vout.resize(1);

  {
    // insufficient amount in deposit
    CTransaction deposit = CreateDepositTx(CTransaction(mtx), key, 10000);
    CValidationState err_state;

    FinalizationState fin_state(FinalizationParams{}, AdminParams{});
    bool ok = ContextualCheckDepositTx(deposit, err_state, fin_state);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-deposit-invalid");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 10);
  }

  {
    // duplicate deposit
    CTransaction deposit = CreateDepositTx(CTransaction(mtx), key, 10000 * UNIT);
    CValidationState err_state;
    FinalizationState fin_state(FinalizationParams{}, AdminParams{});

    bool ok = ContextualCheckDepositTx(deposit, err_state, fin_state);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 0);

    uint160 address;
    BOOST_CHECK(ExtractValidatorAddress(deposit, address));
    fin_state.ProcessDeposit(address, deposit.vout[0].nValue);

    ok = ContextualCheckDepositTx(deposit, err_state, fin_state);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-deposit-duplicate");
    BOOST_CHECK(!err_state.IsValid());

    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 0);
  }
}

BOOST_AUTO_TEST_CASE(ExtractValidatorIndex_logout) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::DEPOSIT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction logout = CreateLogoutTx(prevTx, k, 10000);
  uint160 validatorAddress = uint160();
  BOOST_CHECK(ExtractValidatorAddress(logout, validatorAddress));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetID().GetHex(), validatorAddress.GetHex());
}

BOOST_AUTO_TEST_CASE(ExtractValidatorIndex_withdraw) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::LOGOUT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction withdraw = CreateWithdrawTx(prevTx, k, 10000);
  uint160 validatorAddress = uint160();
  BOOST_CHECK(ExtractValidatorAddress(withdraw, validatorAddress));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetID().GetHex(), validatorAddress.GetHex());
}

BOOST_AUTO_TEST_CASE(ExtractValidatorIndex_p2pkh_fails) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::STANDARD);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction p2pkh = CreateP2PKHTx(prevTx, k, 10000);
  uint160 validatorAddress = uint160();
  BOOST_CHECK(ExtractValidatorAddress(p2pkh, validatorAddress) == false);
}

BOOST_AUTO_TEST_CASE(ExtractValidatorIndex_vote_fails) {

  Vote vote{};

  CKey k;
  InsecureNewKey(k, true);

  CTransaction p2pkh = CreateVoteTx(vote, k);
  uint160 validatorAddress = uint160();
  BOOST_CHECK(ExtractValidatorAddress(p2pkh, validatorAddress) == false);
}

BOOST_AUTO_TEST_SUITE_END()
