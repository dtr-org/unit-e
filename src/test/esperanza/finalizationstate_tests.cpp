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

BOOST_AUTO_TEST_CASE(get_epoch) {
  std::map<uint32_t, uint32_t> height_to_epoch{
      {0, 0},
      {1, 1},
      {2, 1},
      {3, 1},
      {4, 1},
      {5, 1},
      {6, 2},
      {9, 2},
      {10, 2},
      {11, 3},
      {15, 3},
      {16, 4},
      {20, 4},
      {25, 5},
  };

  FinalizationParams params;
  FinalizationState state(params, esperanza::AdminParams{});
  BOOST_REQUIRE_EQUAL(state.GetEpochLength(), 5);

  for (const auto &it : height_to_epoch) {
    BOOST_CHECK_EQUAL(state.GetEpoch(it.first), it.second);
  }
}

// InitializeEpoch tests

BOOST_AUTO_TEST_CASE(initialize_epoch_wrong_height_passed) {

  FinalizationStateSpy state;

  BOOST_CHECK_EQUAL(state.InitializeEpoch(state.EpochLength() * 2 + 1),
                    +Result::INIT_WRONG_EPOCH);
  BOOST_CHECK_EQUAL(0, state.GetCurrentEpoch());
  BOOST_CHECK_EQUAL(0, state.GetCurrentDynasty());
  BOOST_CHECK_EQUAL(0, state.GetLastFinalizedEpoch());
  BOOST_CHECK_EQUAL(0, state.GetLastJustifiedEpoch());
}

BOOST_AUTO_TEST_CASE(initialize_epoch_insta_justify) {

  FinalizationStateSpy spy;
  BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 0);
  BOOST_CHECK_EQUAL(spy.GetLastJustifiedEpoch(), 0);
  BOOST_CHECK_EQUAL(spy.GetLastFinalizedEpoch(), 0);

  Result res = spy.InitializeEpoch(1);
  BOOST_CHECK_EQUAL(res, +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 1);
  BOOST_CHECK_EQUAL(spy.GetLastJustifiedEpoch(), 0);
  BOOST_CHECK_EQUAL(spy.GetLastFinalizedEpoch(), 0);

  res = spy.InitializeEpoch(1 + 1 * spy.EpochLength());
  BOOST_CHECK_EQUAL(res, +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 2);
  BOOST_CHECK_EQUAL(spy.GetLastJustifiedEpoch(), 1);
  BOOST_CHECK_EQUAL(spy.GetLastFinalizedEpoch(), 0);

  res = spy.InitializeEpoch(1 + 2 * spy.EpochLength());
  BOOST_CHECK_EQUAL(res, +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 3);
  BOOST_CHECK_EQUAL(spy.GetLastJustifiedEpoch(), 2);
  BOOST_CHECK_EQUAL(spy.GetLastFinalizedEpoch(), 1);

  res = spy.InitializeEpoch(1 + 3 * spy.EpochLength());
  BOOST_CHECK_EQUAL(res, +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 4);
  BOOST_CHECK_EQUAL(spy.GetLastJustifiedEpoch(), 3);
  BOOST_CHECK_EQUAL(spy.GetLastFinalizedEpoch(), 2);
}

// This tests assumes block time of 4s, hence epochs every 200s, and return of
// 6% per year given that the total deposit of validator is 150Mln units
BOOST_AUTO_TEST_CASE(initialize_epoch_reward_factor) {

  FinalizationStateSpy spy;
  *spy.CurDynDeposits() = 150000000;
  *spy.PrevDynDeposits() = 150000000;

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(1), +Result::SUCCESS);
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
  CBlock block_50;

  CBlock block_100;

  FinalizationStateSpy state_50(state);
  blockIndex.nHeight = 50;
  state_50.ProcessNewTip(blockIndex, block_50);

  FinalizationStateSpy state_51(state_50);
  blockIndex.nHeight = 51;
  state_51.ProcessNewTip(blockIndex, CBlock());

  FinalizationStateSpy state_100(state_51);
  blockIndex.nHeight = 100;
  state_100.ProcessNewTip(blockIndex, CBlock());
  state_100.SetExpectedSourceEpoch(100);

  FinalizationStateSpy state_101(state_100);
  blockIndex.nHeight = 101;
  state_101.ProcessNewTip(blockIndex, CBlock());
  state_101.SetExpectedSourceEpoch(100);

  Vote vote{validatorAddress, block_100.GetHash(), 1, 2};
  CTransactionRef voteTx = MakeTransactionRef(CreateVoteTx(vote, k));
  block.vtx = std::vector<CTransactionRef>{voteTx};
  uint256 voteHash = voteTx->GetHash();

  FinalizationStateSpy state_102(state_101);
  blockIndex.nHeight = 102;
  state_102.ProcessNewTip(blockIndex, block);
  BOOST_CHECK_EQUAL(voteHash.GetHex(),
                    state_102.GetLastTxHash(validatorAddress).GetHex());

  // Test logout
  CTransactionRef logoutTx =
      MakeTransactionRef(CreateLogoutTx(*voteTx, k, depositTx->vout[0].nValue));

  block.vtx = std::vector<CTransactionRef>{logoutTx};

  FinalizationStateSpy state_103(state_102);
  blockIndex.nHeight = 103;
  state_103.ProcessNewTip(blockIndex, block);

  uint256 logoutHash = logoutTx->GetHash();
  BOOST_CHECK_EQUAL(logoutHash.GetHex(),
                    state_103.GetLastTxHash(validatorAddress).GetHex());
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
