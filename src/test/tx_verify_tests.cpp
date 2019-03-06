// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/block_builder.h>

#include <consensus/tx_verify.h>
#include <consensus/validation.h>

#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
#include <boost/test/unit_test.hpp>

#include <algorithm>

BOOST_FIXTURE_TEST_SUITE(tx_verify_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(check_tx_inputs_no_haz_coins) {

  mocks::CoinsViewMock utxos;
  utxos.default_have_inputs = false;

  CTransaction tx;
  CValidationState validation_state;
  CAmount fees;

  const bool result = Consensus::CheckTxInputs(tx, validation_state, utxos, 2, fees);
  BOOST_CHECK(!result);
  BOOST_CHECK(!validation_state.IsValid());
  BOOST_CHECK_EQUAL(validation_state.GetRejectCode(), REJECT_INVALID);
  BOOST_CHECK_EQUAL(validation_state.GetRejectReason(), "bad-txns-inputs-missingorspent");
}

BOOST_AUTO_TEST_CASE(check_tx_inputs_no_reward) {

  const CTxIn meta_input;
  BOOST_REQUIRE(meta_input.prevout.IsNull());
  const uint256 some_txid = uint256S("4623a9438473459c466ea4fe87b5a614362e08c47454cf59646e49c5759cb60d");
  const CTxIn staking_input(some_txid, 0);

  CTransaction tx = [&] {
    CMutableTransaction tx;
    tx.SetType(TxType::COINBASE);
    tx.vin = {meta_input, staking_input};
    return tx;
  }();

  mocks::CoinsViewMock utxos;
  utxos.default_coin = Coin(CTxOut(21, CScript()), 1, false);

  CValidationState validation_state;
  CAmount fees;

  const bool result = Consensus::CheckTxInputs(tx, validation_state, utxos, 2, fees);
  BOOST_CHECK(!result);
  BOOST_CHECK(!validation_state.IsValid());
  BOOST_CHECK_EQUAL(validation_state.GetRejectCode(), REJECT_INVALID);
  BOOST_CHECK_EQUAL(validation_state.GetRejectReason(), "bad-cb-no-reward");
}

BOOST_AUTO_TEST_CASE(check_tx_inputs_does_not_access_coinbase_meta_input) {

  const CTxIn meta_input;
  BOOST_REQUIRE(meta_input.prevout.IsNull());
  const uint256 some_txid = uint256S("4623a9438473459c466ea4fe87b5a614362e08c47454cf59646e49c5759cb60d");
  const CTxIn staking_input(some_txid, 0);

  CTransaction tx = [&] {
    CMutableTransaction tx;
    tx.SetType(TxType::COINBASE);
    tx.vin = {meta_input, staking_input};
    tx.vout = {CTxOut(21, CScript())};
    return tx;
  }();

  std::vector<COutPoint> coins_accessed;
  mocks::CoinsViewMock utxos;
  utxos.default_coin = Coin(CTxOut(21, CScript()), 1, false);
  utxos.access_coin = [&](const COutPoint &coin) -> const Coin & {
    coins_accessed.emplace_back(coin);
    return utxos.default_coin;
  };

  CValidationState validation_state;
  CAmount fees;

  Consensus::CheckTxInputs(tx, validation_state, utxos, 2, fees);
  BOOST_CHECK(std::find(coins_accessed.begin(), coins_accessed.end(), meta_input.prevout) == coins_accessed.end());
}

BOOST_AUTO_TEST_CASE(check_tx_inputs_rejects_coinbase_that_spends_too_little) {

  const CTxIn meta_input;
  BOOST_REQUIRE(meta_input.prevout.IsNull());
  const uint256 some_txid = uint256S("4623a9438473459c466ea4fe87b5a614362e08c47454cf59646e49c5759cb60d");
  const CTxIn staking_input(some_txid, 0);

  const CAmount reward = 21;
  const CAmount stake_in = 19;
  const CAmount stake_out = stake_in - 1;

  CTransaction tx = [&] {
    CMutableTransaction tx;
    tx.SetType(TxType::COINBASE);
    tx.vin = {meta_input, staking_input};
    tx.vout = {CTxOut(reward, CScript()), CTxOut(stake_out, CScript())};
    return tx;
  }();

  mocks::CoinsViewMock utxos;
  utxos.default_coin = Coin(CTxOut(stake_in, CScript()), 1, false);

  CValidationState validation_state;
  CAmount fees;

  const bool result = Consensus::CheckTxInputs(tx, validation_state, utxos, 2, fees);
  BOOST_CHECK(!result);
  BOOST_CHECK(!validation_state.IsValid());
  BOOST_CHECK_EQUAL(validation_state.GetRejectCode(), REJECT_INVALID);
  BOOST_CHECK_EQUAL(validation_state.GetRejectReason(), "bad-cb-spends-too-little");
}

BOOST_AUTO_TEST_CASE(check_tx_inputs_rejects_coinbase_that_spends_too_much) {

  const CTxIn meta_input;
  BOOST_REQUIRE(meta_input.prevout.IsNull());
  const uint256 some_txid = uint256S("4623a9438473459c466ea4fe87b5a614362e08c47454cf59646e49c5759cb60d");
  const CTxIn staking_input(some_txid, 0);

  const CAmount reward = 21;
  const CAmount stake_in = 19;
  const CAmount stake_out = stake_in + 1;

  CTransaction tx = [&] {
    CMutableTransaction tx;
    tx.SetType(TxType::COINBASE);
    tx.vin = {meta_input, staking_input};
    tx.vout = {CTxOut(reward, CScript()), CTxOut(stake_out, CScript())};
    return tx;
  }();

  mocks::CoinsViewMock utxos;
  utxos.default_coin = Coin(CTxOut(stake_in, CScript()), 1, false);

  CValidationState validation_state;
  CAmount fees;

  const bool result = Consensus::CheckTxInputs(tx, validation_state, utxos, 2, fees);
  BOOST_CHECK(!result);
  BOOST_CHECK(!validation_state.IsValid());
  BOOST_CHECK_EQUAL(validation_state.GetRejectCode(), REJECT_INVALID);
  BOOST_CHECK_EQUAL(validation_state.GetRejectReason(), "bad-cb-spends-too-much");
}

BOOST_AUTO_TEST_SUITE_END()
