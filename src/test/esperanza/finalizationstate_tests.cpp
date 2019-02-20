// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/state_storage.h>
#include <injector.h>
#include <keystore.h>
#include <test/esperanza/finalization_utils.h>
#include <test/esperanza/finalizationstate_utils.h>
#include <test/test_unite_mocks.h>
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
  spy.SetRecommendedTarget(&target);
  spy.SetExpectedSourceEpoch(3);

  Vote res = spy.GetRecommendedVote(validator_address);

  BOOST_CHECK_EQUAL(res.m_validatorAddress.GetHex(), validator_address.GetHex());
  BOOST_CHECK_EQUAL(res.m_sourceEpoch, 3);
  BOOST_CHECK_EQUAL(res.m_targetEpoch, 7);
  BOOST_CHECK_EQUAL(res.m_targetHash, target_hash);
}

BOOST_AUTO_TEST_CASE(register_last_validator_tx) {
  FinalizationStateSpy state;

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

namespace {

class StorageFixture {
 public:
  static constexpr blockchain::Height epoch_length = 5;

  StorageFixture() : m_storage(finalization::StateStorage::New(&m_chain)) {
    m_finalization_params = Params().GetFinalization();
    m_finalization_params.epoch_length = epoch_length;
    m_admin_params = Params().GetAdminParams();
    m_storage->Reset(m_finalization_params, m_admin_params);
    m_chain.block_at_height = [this](blockchain::Height h) -> CBlockIndex * {
      auto const it = this->m_block_heights.find(h);
      BOOST_REQUIRE(it != this->m_block_heights.end());
      return it->second;
    };
  }

  CBlockIndex *GenerateBlockIndex() {
    const auto height = FindNextHeight();
    const auto ins_res = m_block_indexes.emplace(uint256S(std::to_string(height)), new CBlockIndex);
    CBlockIndex *index = ins_res.first->second;
    index->nHeight = height;
    index->phashBlock = &ins_res.first->first;
    index->pprev = m_chain.tip;
    m_chain.tip = index;
    m_block_heights[index->nHeight] = index;
    return index;
  }

  bool ProcessNewCommits(const CBlockIndex *block_index) {
    return m_storage->ProcessNewCommits(*block_index, {});
  }

  bool ProcessNewTipCandidate(const CBlockIndex *block_index) {
    return m_storage->ProcessNewTipCandidate(*block_index, CBlock());
  }

  bool ProcessNewTip(const CBlockIndex *block_index) {
    return m_storage->ProcessNewTip(*block_index, CBlock());
  }

  void AddBlock() {
    const auto *block_index = GenerateBlockIndex();
    const bool process_res = ProcessNewTip(block_index);
    BOOST_REQUIRE(process_res);
  }

  void AddBlocks(size_t amount) {
    for (size_t i = 0; i < amount; ++i) {
      AddBlock();
    }
  }

  const esperanza::FinalizationState *GetState(const blockchain::Height h) {
    return m_storage->GetState(*m_chain.AtHeight(h));
  }

  const esperanza::FinalizationState *GetState(const CBlockIndex *block_index) {
    return m_storage->GetState(*block_index);
  }

 private:
  blockchain::Height FindNextHeight() {
    if (m_chain.tip == nullptr) {
      return 0;
    } else {
      return m_chain.GetTip()->nHeight + 1;
    }
  }

  esperanza::FinalizationParams m_finalization_params;
  esperanza::AdminParams m_admin_params;
  std::map<uint256, CBlockIndex *> m_block_indexes;
  std::map<blockchain::Height, CBlockIndex *> m_block_heights;
  mocks::ActiveChainMock m_chain;
  std::unique_ptr<finalization::StateStorage> m_storage;
};
}  // namespace

BOOST_AUTO_TEST_CASE(storage_trimming) {
  StorageFixture fixture;
  BOOST_REQUIRE(fixture.epoch_length == 5);

  // Generate first epoch
  fixture.AddBlocks(5);

  // Check, all states presented in the storage
  BOOST_CHECK(fixture.GetState(blockchain::Height(0)) != nullptr);
  BOOST_CHECK(fixture.GetState(1) != nullptr);
  BOOST_CHECK(fixture.GetState(2) != nullptr);
  BOOST_CHECK(fixture.GetState(3) != nullptr);
  BOOST_CHECK(fixture.GetState(4) != nullptr);

  // Check, states are different
  BOOST_CHECK(fixture.GetState(blockchain::Height(0)) != fixture.GetState(1));
  BOOST_CHECK(fixture.GetState(1) != fixture.GetState(4));

  // Generate one more block, trigger finalization of previous epoch
  fixture.AddBlocks(1);

  // Now epoch 1 is finalized, check old states disappear from the storage
  BOOST_CHECK(fixture.GetState(blockchain::Height(0)) != nullptr);  // genesis
  BOOST_CHECK(fixture.GetState(1) == nullptr);
  BOOST_CHECK(fixture.GetState(2) == nullptr);
  BOOST_CHECK(fixture.GetState(3) == nullptr);
  BOOST_CHECK(fixture.GetState(4) != nullptr);  // finalized checkpoint
  BOOST_CHECK(fixture.GetState(5) != nullptr);  // first block of new epoch

  // Complete current epoch
  fixture.AddBlocks(4);

  // Check, new states are in the storage
  BOOST_CHECK(fixture.GetState(4) != nullptr);
  BOOST_CHECK(fixture.GetState(5) != nullptr);
  BOOST_CHECK(fixture.GetState(9) != nullptr);

  // Generate next epoch. We haven't reached finalization yet.
  fixture.AddBlocks(5);
  BOOST_CHECK(fixture.GetState(4) != nullptr);
  BOOST_CHECK(fixture.GetState(5) != nullptr);
  BOOST_CHECK(fixture.GetState(9) != nullptr);

  // Generate one more block, trigger finalization of the first epoch
  fixture.AddBlocks(1);

  BOOST_CHECK(fixture.GetState(4) == nullptr);
  BOOST_CHECK(fixture.GetState(8) == nullptr);
  BOOST_CHECK(fixture.GetState(9) != nullptr);
  BOOST_CHECK(fixture.GetState(10) != nullptr);
}

BOOST_AUTO_TEST_CASE(storage_states_workflow) {
  StorageFixture fixture;
  BOOST_REQUIRE(fixture.epoch_length == 5);

  // Generate first epoch
  fixture.AddBlocks(5);

  bool ok = false;
  const auto *block_index = fixture.GenerateBlockIndex();

  // Process state from commits. It't not confirmed yet, finalization shouldn't happen.
  ok = fixture.ProcessNewCommits(block_index);
  BOOST_REQUIRE(ok);
  BOOST_CHECK(fixture.GetState(block_index)->GetStatus() == esperanza::FinalizationState::FROM_COMMITS);
  BOOST_CHECK(fixture.GetState(1) != nullptr);

  // Process the same state from the block, it must be confirmed now. As it's not yet considered as
  // a prt of the main chain, finalization shouldn't happen.
  ok = fixture.ProcessNewTipCandidate(block_index);
  BOOST_REQUIRE(ok);
  BOOST_CHECK(fixture.GetState(block_index)->GetStatus() == esperanza::FinalizationState::CONFIRMED);
  BOOST_CHECK(fixture.GetState(1) != nullptr);

  // Process the same state from the block and consider it as a part of the main chain so that expect
  // finalization and trimming the storage.
  ok = fixture.ProcessNewTip(block_index);
  BOOST_REQUIRE(ok);
  BOOST_CHECK(fixture.GetState(block_index)->GetStatus() == esperanza::FinalizationState::CONFIRMED);
  BOOST_CHECK(fixture.GetState(1) == nullptr);

  // Generate two more indexes
  const auto *b1 = fixture.GenerateBlockIndex();
  const auto *b2 = fixture.GenerateBlockIndex();

  // Try to process new state for b2. This should fail due to we haven't processed state for b1 yet.
  ok = fixture.ProcessNewCommits(b2);
  BOOST_CHECK_EQUAL(ok, false);
  ok = fixture.ProcessNewTipCandidate(b2);
  BOOST_CHECK_EQUAL(ok, false);
  ok = fixture.ProcessNewTip(b2);
  BOOST_CHECK_EQUAL(ok, false);

  // Process b1 state from commits and try to process b2 from block. This must fail due to we can't
  // confirm state that based on unconfirmed one.
  ok = fixture.ProcessNewCommits(b1);
  BOOST_REQUIRE(ok);
  ok = fixture.ProcessNewTipCandidate(b2);
  BOOST_CHECK_EQUAL(ok, false);
  ok = fixture.ProcessNewTip(b2);
  BOOST_CHECK_EQUAL(ok, false);

  // Now we can process b2 from commits and then from the block (it's what we do in snapshot sync).
  ok = fixture.ProcessNewCommits(b2);
  BOOST_CHECK_EQUAL(ok, true);
  ok = fixture.ProcessNewTip(b2);
  BOOST_CHECK_EQUAL(ok, true);

  // Process next block as usual
  fixture.AddBlocks(1);
}

BOOST_AUTO_TEST_SUITE_END()
