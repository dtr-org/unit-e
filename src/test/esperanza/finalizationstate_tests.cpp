// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <keystore.h>
#include <test/esperanza/finalization_utils.h>
#include <test/esperanza/finalizationstate_utils.h>
#include <ufp64.h>
#include <util.h>

using namespace esperanza;

BOOST_FIXTURE_TEST_SUITE(finalizationstate_tests, ReducedTestingSetup)

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

  for (int i = 0; i < spy.EpochLength() * 3; i++) {
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

  FinalizationState::Init(FinalizationParams(), AdminParams());
  auto state = FinalizationState::GetState();

  uint160 validatorAddress = uint160S("00112233445566778899");

  CKey k;
  InsecureNewKey(k, true);

  CBlockIndex blockIndex;
  CBlock block;

  CMutableTransaction tx;
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransactionRef deposit = MakeTransactionRef(CreateDepositTx(tx, k, 10000));
  block.vtx = std::vector<CTransactionRef>{deposit};

  uint256 depositHash = deposit->GetHash();
  FinalizationState::ProcessNewTip(blockIndex, block);
  BOOST_CHECK_EQUAL(depositHash.GetHex(), state->GetLastTxHash(validatorAddress).GetHex());

  //  CTransactionRef vote = &CreateVoteTx();
  //  block.vtx = std::vector<CTransactionRef>{vote};
  //  uint256 voteHash = vote->GetHash();
  //  FinalizationState::ProcessNewTip(blockIndex, block);
  //  BOOST_CHECK_EQUAL(voteHash.GetHex(), state->GetLastTxHash(validatorAddress));
  //
  //  CTransactionRef logout = &CreateLogoutTx();
  //  block.vtx = std::vector<CTransactionRef>{logout};
  //  uint256 logoutHash = logout->GetHash();
  //  FinalizationState::ProcessNewTip(blockIndex, block);
  //  BOOST_CHECK_EQUAL(logoutHash.GetHex(), state->GetLastTxHash(validatorAddress));
  //
  //  CTransactionRef withdraw = &CreateWithdrawTx();
  //  block.vtx = std::vector<CTransactionRef>{withdraw};
  //  uint256 withdrawHash = withdraw->GetHash();
  //  FinalizationState::ProcessNewTip(blockIndex, block);
  //  BOOST_CHECK_EQUAL(withdrawHash.GetHex(), state->GetLastTxHash(validatorAddress));
}

// Other tests

BOOST_AUTO_TEST_CASE(map_empty_initializer) {
  std::map<uint32_t, uint32_t> map;

  for (int i = 0; i < 100; i++) {
    BOOST_CHECK_EQUAL(0, map[i]);
  }
}

BOOST_AUTO_TEST_SUITE_END()
