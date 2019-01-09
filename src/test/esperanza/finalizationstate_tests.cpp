// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

BOOST_AUTO_TEST_CASE(initialize_epoch_insta_finalize) {

  FinalizationStateSpy spy;

  for (uint32_t i = 0; i < spy.EpochLength() * 3; i++) {
    if (i < spy.EpochLength()) {
      BOOST_CHECK_EQUAL(spy.InitializeEpoch(i), +Result::INIT_WRONG_EPOCH);
    } else {
      if (i % spy.EpochLength() == 0) {
        BOOST_CHECK_EQUAL(spy.InitializeEpoch(i), +Result::SUCCESS);
      }

      int expectedEpoch = i / spy.EpochLength();
      int expectedDynasty = (i / spy.EpochLength()) - 1;

      BOOST_CHECK_EQUAL(expectedEpoch, spy.GetCurrentEpoch());
      BOOST_CHECK_EQUAL(expectedDynasty, spy.GetCurrentDynasty());
      BOOST_CHECK_EQUAL(expectedDynasty, spy.GetLastFinalizedEpoch());
      BOOST_CHECK_EQUAL(expectedDynasty, spy.GetLastJustifiedEpoch());
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
BOOST_AUTO_TEST_CASE(getrecommendedvote) {

  FinalizationStateSpy spy;
  uint160 validatorAddress = RandValidatorAddr();
  CAmount depositSize = spy.MinDepositSize();

  BOOST_CHECK_EQUAL(spy.ValidateDeposit(validatorAddress, depositSize),
                    +Result::SUCCESS);
  spy.ProcessDeposit(validatorAddress, depositSize);

  BOOST_CHECK_EQUAL(spy.InitializeEpoch(spy.EpochLength()), +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(2 * spy.EpochLength()),
                    +Result::SUCCESS);
  BOOST_CHECK_EQUAL(spy.InitializeEpoch(3 * spy.EpochLength()),
                    +Result::SUCCESS);

  int i;
  for (i = 4; i < 8; i++) {
    BOOST_CHECK_EQUAL(spy.InitializeEpoch(i * spy.EpochLength()),
                      +Result::SUCCESS);
  }

  // For simplicity we keep the targetHash constant since it does not
  // affect the state.
  uint256 targetHash = GetRandHash();
  *spy.RecommendedTargetHash() = targetHash;

  Vote res = spy.GetRecommendedVote(validatorAddress);

  BOOST_CHECK_EQUAL(res.m_validatorAddress.GetHex(), validatorAddress.GetHex());
  BOOST_CHECK_EQUAL(res.m_sourceEpoch, 3);
  BOOST_CHECK_EQUAL(res.m_targetEpoch, 7);
  BOOST_CHECK_EQUAL(res.m_targetHash, targetHash);
}

BOOST_AUTO_TEST_CASE(register_last_validator_tx) {

  auto state = FinalizationState::GetState();

  CKey k;
  InsecureNewKey(k, true);

  uint160 validatorAddress = k.GetPubKey().GetID();

  uint256 block_hash;
  CBlockIndex blockIndex;
  blockIndex.nHeight = 1;
  blockIndex.phashBlock = &block_hash;
  CBlock block;

  CMutableTransaction tx;
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  // Test deposit
  CTransactionRef depositTx = MakeTransactionRef(CreateDepositTx(tx, k, 10000));
  block.vtx = std::vector<CTransactionRef>{depositTx};

  uint256 depositHash = depositTx->GetHash();
  state->ProcessNewTip(blockIndex, block);

  BOOST_CHECK_EQUAL(depositHash.GetHex(),
                    state->GetLastTxHash(validatorAddress).GetHex());

  // Test vote
  CBlock block_49;
  block_49.nNonce = 1;

  CBlock block_99;
  block_99.nNonce = 2;

  blockIndex.nHeight = 49;
  state->ProcessNewTip(blockIndex, block_49);

  blockIndex.nHeight = 50;
  state->ProcessNewTip(blockIndex, CBlock());

  blockIndex.nHeight = 99;
  state->ProcessNewTip(blockIndex, block_99);

  blockIndex.nHeight = 100;
  state->ProcessNewTip(blockIndex, CBlock());

  Vote vote{validatorAddress, block_99.GetHash(), 1, 2};
  CTransactionRef voteTx = MakeTransactionRef(CreateVoteTx(vote, k));
  block.vtx = std::vector<CTransactionRef>{voteTx};
  uint256 voteHash = voteTx->GetHash();
  blockIndex.nHeight = 101;
  state->ProcessNewTip(blockIndex, block);
  BOOST_CHECK_EQUAL(voteHash.GetHex(),
                    state->GetLastTxHash(validatorAddress).GetHex());

  // Test logout
  CTransactionRef logoutTx =
      MakeTransactionRef(CreateLogoutTx(*voteTx, k, depositTx->vout[0].nValue));

  block.vtx = std::vector<CTransactionRef>{logoutTx};
  uint256 logoutHash = logoutTx->GetHash();
  state->ProcessNewTip(blockIndex, block);
  BOOST_CHECK_EQUAL(logoutHash.GetHex(),
                    state->GetLastTxHash(validatorAddress).GetHex());
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

  FinalizationState *state = FinalizationState::GetState();
  state->ProcessNewTip(blockIndex, block);

  BOOST_CHECK_EQUAL(10000, state->GetDepositSize(validatorAddress));
}

namespace {
CBlockIndex *AddBlock(CBlockIndex *prev, int i) {
  auto res = mapBlockIndex.emplace(uint256S(std::to_string(1000 + i)), new CBlockIndex);
  CBlockIndex &index = *res.first->second;
  index.nHeight = i;
  index.phashBlock = &res.first->first;
  index.pprev = prev;
  chainActive.SetTip(&index);
  esperanza::ProcessNewTip(index, CBlock());
  return &index;
}
}  // namespace

BOOST_AUTO_TEST_CASE(storage) {
  const auto epoch_length = static_cast<int>(esperanza::GetEpochLength());
  CBlockIndex *prev = chainActive.Genesis();
  int i = 1;
  // Generate first epoch block
  for (; i <= epoch_length - 1; ++i) {
    prev = AddBlock(prev, i);
  }
  // Check, all states presented in the cache
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[0]) != nullptr);
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[1]) != nullptr);
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[epoch_length / 2]) != nullptr);
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[epoch_length - 1]) != nullptr);

  // Check, states are different
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[0]) != esperanza::FinalizationState::GetState(chainActive[1]));
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[1]) != esperanza::FinalizationState::GetState(chainActive[epoch_length - 1]));

  // generate one more block, trigger finalization of previous epoch
  prev = AddBlock(prev, epoch_length);
  ++i;

  // Now epoch 1 is finalized, check old states disappear from the cache
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[0]) != nullptr);  // genesis
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[1]) == nullptr);
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[epoch_length / 2]) == nullptr);
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[epoch_length - 1]) != nullptr);  // finalized checkpoint
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[epoch_length]) != nullptr);      // first block of new epoch

  // Generate next epoch
  for (; i <= epoch_length * 2 - 1; ++i) {
    prev = AddBlock(prev, i);
  }

  // Check, new states are in the cache
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[epoch_length]) != nullptr);
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[epoch_length * 2 - 1]) != nullptr);

  // generate one more block, trigger finalization of previous epoch
  prev = AddBlock(prev, epoch_length * 2);
  ++i;

  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[epoch_length]) == nullptr);
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[epoch_length * 2 - 2]) == nullptr);
  BOOST_CHECK(esperanza::FinalizationState::GetState(chainActive[epoch_length * 2 - 1]) != nullptr);  // finalized checkpoint
}

BOOST_AUTO_TEST_SUITE_END()
