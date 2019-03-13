// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <injector.h>
#include <keystore.h>
#include <test/esperanza/finalization_utils.h>
#include <test/esperanza/finalizationstate_utils.h>
#include <ufp64.h>
#include <util.h>
#include <validation.h>

using namespace esperanza;

BOOST_FIXTURE_TEST_SUITE(finalizationstate_tests, TestingSetup)

// Constructor tests

BOOST_AUTO_TEST_CASE(constructor) {

  FinalizationStateSpy state;

  BOOST_CHECK_EQUAL(0, state.GetCurrentEpoch());
  BOOST_CHECK_EQUAL(0, state.GetCurrentDynasty());
  BOOST_CHECK_EQUAL(0, state.GetLastFinalizedEpoch());
  BOOST_CHECK_EQUAL(0, state.GetLastJustifiedEpoch());
}

// InitializeEpoch tests

BOOST_AUTO_TEST_CASE(initialize_epoch_wrong_height_passed) {

  FinalizationStateSpy state;

  BOOST_CHECK_EQUAL(state.InitializeEpoch(2 * state.EpochLength()),
                    +Result::INIT_WRONG_EPOCH);
  BOOST_CHECK_EQUAL(state.InitializeEpoch(state.EpochLength() - 1),
                    +Result::INIT_WRONG_EPOCH);
  BOOST_CHECK_EQUAL(0, state.GetCurrentEpoch());
  BOOST_CHECK_EQUAL(0, state.GetCurrentDynasty());
  BOOST_CHECK_EQUAL(0, state.GetLastFinalizedEpoch());
  BOOST_CHECK_EQUAL(0, state.GetLastJustifiedEpoch());
}

BOOST_AUTO_TEST_CASE(initialize_epoch_insta_justify) {

  FinalizationStateSpy spy;

  for (uint32_t i = 0; i < spy.EpochLength() * 3; ++i) {
    if (i < spy.EpochLength()) {
      BOOST_CHECK_EQUAL(spy.InitializeEpoch(i), +Result::INIT_WRONG_EPOCH);
    } else {
      if (i % spy.EpochLength() == 0) {
        BOOST_CHECK_EQUAL(spy.InitializeEpoch(i), +Result::SUCCESS);
      }

      const uint32_t current_epoch = i / spy.EpochLength();
      BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), current_epoch);
      BOOST_CHECK_EQUAL(spy.GetCurrentDynasty(), current_epoch > 2 ? current_epoch - 3 : 0);
      BOOST_CHECK_EQUAL(spy.GetLastFinalizedEpoch(), current_epoch > 1 ? current_epoch - 2 : 0);
      BOOST_CHECK_EQUAL(spy.GetLastJustifiedEpoch(), current_epoch - 1);
    }
  }
}

// This tests assumes block time of 4s, hence epochs every 200s, and return of
// 6% per year given that the total deposit of validator is 150Mln units
BOOST_AUTO_TEST_CASE(initialize_epoch_reward_factor) {

  FinalizationStateSpy spy;
  *spy.CurDynDeposits() = 150000000;
  *spy.PrevDynDeposits() = 150000000;

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL("0.00057174", ufp64::to_str(*spy.RewardFactor()));
}

// GetRecommendedVote tests
BOOST_AUTO_TEST_CASE(get_recommended_vote) {
  FinalizationStateSpy spy;
  uint160 validator_address = RandValidatorAddr();

  const uint256 target_hash = GetRandHash();
  CBlockIndex target;
  target.phashBlock = &target_hash;
  target.nHeight = 7 * spy.EpochLength();
  spy.SetRecommendedTarget(target);
  spy.SetExpectedSourceEpoch(3);

  Vote res = spy.GetRecommendedVote(validator_address);

  BOOST_CHECK_EQUAL(res.m_validator_address.GetHex(), validator_address.GetHex());
  BOOST_CHECK_EQUAL(res.m_source_epoch, 3);
  BOOST_CHECK_EQUAL(res.m_target_epoch, 7);
  BOOST_CHECK_EQUAL(res.m_target_hash, target_hash);
}

BOOST_AUTO_TEST_CASE(register_last_validator_tx) {
  FinalizationStateSpy state;

  CKey k;
  InsecureNewKey(k, true);

  uint160 validatorAddress = k.GetPubKey().GetID();
  uint256 blockhash;

  uint256 block_hash;
  CBlockIndex blockIndex;
  blockIndex.phashBlock = &blockhash;
  blockIndex.nHeight = 1;
  CBlock block;

  CMutableTransaction tx;
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  // Test deposit
  CTransactionRef depositTx = MakeTransactionRef(CreateDepositTx(tx, k, 10000));
  block.vtx = std::vector<CTransactionRef>{depositTx};

  uint256 depositHash = depositTx->GetHash();
  state.ProcessNewTip(blockIndex, block);

  BOOST_CHECK_EQUAL(depositHash.GetHex(),
                    state.GetLastTxHash(validatorAddress).GetHex());

  // Test vote
  CBlock block_49;
  block_49.nNonce = 1;

  CBlock block_99;
  block_99.nNonce = 2;

  FinalizationStateSpy state_49(state);
  blockIndex.nHeight = 49;
  state_49.ProcessNewTip(blockIndex, block_49);

  FinalizationStateSpy state_50(state_49);
  blockIndex.nHeight = 50;
  state_50.ProcessNewTip(blockIndex, CBlock());

  FinalizationStateSpy state_99(state_50);
  blockIndex.nHeight = 99;
  state_99.ProcessNewTip(blockIndex, block_99);

  FinalizationStateSpy state_100(state_99);
  blockIndex.nHeight = 100;
  state_100.ProcessNewTip(blockIndex, CBlock());
  state_100.SetExpectedSourceEpoch(100);

  Vote vote{validatorAddress, block_99.GetHash(), 1, 2};
  CTransactionRef voteTx = MakeTransactionRef(CreateVoteTx(vote, k));
  block.vtx = std::vector<CTransactionRef>{voteTx};
  uint256 voteHash = voteTx->GetHash();

  FinalizationStateSpy state_101(state_100);
  blockIndex.nHeight = 101;
  state_101.ProcessNewTip(blockIndex, block);
  BOOST_CHECK_EQUAL(voteHash.GetHex(),
                    state_101.GetLastTxHash(validatorAddress).GetHex());

  // Test logout
  CTransactionRef logoutTx =
      MakeTransactionRef(CreateLogoutTx(*voteTx, k, depositTx->vout[0].nValue));

  block.vtx = std::vector<CTransactionRef>{logoutTx};

  FinalizationStateSpy state_102(state_101);
  blockIndex.nHeight = 102;
  state_102.ProcessNewTip(blockIndex, block);

  uint256 logoutHash = logoutTx->GetHash();
  BOOST_CHECK_EQUAL(logoutHash.GetHex(),
                    state_102.GetLastTxHash(validatorAddress).GetHex());
}

BOOST_AUTO_TEST_CASE(deposit_amount) {

  CKey k;
  InsecureNewKey(k, true);

  uint160 validatorAddress = k.GetPubKey().GetID();

  uint256 block_hash;
  CBlockIndex blockIndex;
  blockIndex.nHeight = 1;
  blockIndex.phashBlock = &block_hash;
  CBlock block;

  CMutableTransaction base_tx;
  base_tx.vin.resize(1);
  base_tx.vout.resize(1);

  CMutableTransaction deposit_tx = CreateDepositTx(base_tx, k, 10000);
  deposit_tx.vout.emplace_back(15000, CScript::CreateP2PKHScript(ToByteVector(validatorAddress)));

  block.vtx = std::vector<CTransactionRef>{MakeTransactionRef(deposit_tx)};

  FinalizationStateSpy state;
  state.ProcessNewTip(blockIndex, block);

  BOOST_CHECK_EQUAL(10000, state.GetDepositSize(validatorAddress));
}

BOOST_AUTO_TEST_SUITE_END()
