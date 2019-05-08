// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/esperanza/finalizationstate_utils.h>

BOOST_FIXTURE_TEST_SUITE(finalizationstate_calculate_withdraw_amount_tests, TestingSetup)

void CreateAndProcessVote(FinalizationStateSpy &state, uint160 finalizer_address, uint256 target_hash) {
  Vote vote{finalizer_address, target_hash, state.GetExpectedSourceEpoch(), state.GetRecommendedTargetEpoch()};
  BOOST_REQUIRE_EQUAL(state.ValidateVote(vote), +Result::SUCCESS);
  uint32_t prev_justified = state.GetLastJustifiedEpoch();
  state.ProcessVote(vote);
  BOOST_REQUIRE_EQUAL(state.GetLastJustifiedEpoch(), prev_justified + 1);
}

void InitializeNextEpoch(FinalizationStateSpy &state) {
  state.SetRecommendedTargetEpoch(state.GetCurrentEpoch());

  Result res = state.InitializeEpoch(1 + state.GetCurrentEpoch() * state.EpochLength());
  BOOST_REQUIRE_EQUAL(res, +Result::SUCCESS);
}

BOOST_AUTO_TEST_CASE(calculate_withdraw_amount) {
  struct TestCase {
    uint32_t epochs_before_logout;
    CAmount deposit_amount;
    CAmount withdraw_amount;
  };

  std::vector<TestCase> test_cases{
      TestCase{
          0,
          1000000000000,
          1000011069779,
      },
      TestCase{
          1,
          1000000000000,
          1000014759724,
      },
      TestCase{
          10,
          1000000000000,
          1000047969221,
      },
      TestCase{
          100,
          1000000000000,
          1000380063832,
      },
  };

  for (size_t test_idx = 0; test_idx < test_cases.size(); ++test_idx) {
    TestCase test_case = test_cases[test_idx];

    // setup
    finalization::Params params = finalization::Params::TestNet();
    FinalizationStateSpy state(params);

    // mock target hash
    uint256 target_hash = GetRandHash();
    CBlockIndex block_index;
    block_index.phashBlock = &target_hash;
    state.SetRecommendedTarget(block_index);

    // deposit
    uint160 finalizer_address = RandValidatorAddr();
    state.CreateAndActivateDeposit(finalizer_address, test_case.deposit_amount);

    // vote before logout
    uint32_t end = state.GetCurrentEpoch() + test_case.epochs_before_logout;
    for (uint32_t i = state.GetCurrentEpoch(); i < end; ++i) {
      CreateAndProcessVote(state, finalizer_address, target_hash);
      InitializeNextEpoch(state);
    }

    // logout
    BOOST_REQUIRE_EQUAL(state.ValidateLogout(finalizer_address), +Result::SUCCESS);
    state.ProcessLogout(finalizer_address);
    BOOST_REQUIRE_EQUAL(state.GetCurrentEpoch(), 4 + test_case.epochs_before_logout);
    BOOST_REQUIRE_EQUAL(state.GetCurrentDynasty(), 2 + test_case.epochs_before_logout);

    // vote during logout delay
    uint32_t end_logout = state.GetCurrentEpoch() + static_cast<uint32_t>(state.DynastyLogoutDelay());
    BOOST_REQUIRE_EQUAL(end_logout, 9 + test_case.epochs_before_logout);

    const esperanza::Validator *finalizer = state.GetValidator(finalizer_address);
    BOOST_REQUIRE(finalizer);

    for (uint32_t i = state.GetCurrentEpoch(); i <= end_logout; ++i) {
      CreateAndProcessVote(state, finalizer_address, target_hash);
      InitializeNextEpoch(state);
    }

    // wait withdraw delay
    uint32_t end_withdraw = end_logout + 1 + static_cast<uint32_t>(state.WithdrawalEpochDelay());
    BOOST_REQUIRE_EQUAL(end_withdraw, 20 + test_case.epochs_before_logout);

    for (uint32_t i = state.GetCurrentEpoch(); i < end_withdraw; ++i) {
      BOOST_REQUIRE_EQUAL(state.ValidateWithdraw(finalizer_address, test_case.deposit_amount), +Result::WITHDRAW_TOO_EARLY);
      CAmount amount;
      BOOST_REQUIRE_EQUAL(state.CalculateWithdrawAmount(finalizer_address, amount), +Result::WITHDRAW_TOO_EARLY);
      InitializeNextEpoch(state);
    }

    // test amount
    BOOST_REQUIRE_EQUAL(state.ValidateWithdraw(finalizer_address, test_case.deposit_amount), +Result::SUCCESS);
    BOOST_REQUIRE_EQUAL(state.GetLastFinalizedEpoch(), state.GetCurrentEpoch() - 1);

    for (uint32_t i = 0; i < 3; ++i) {
      CAmount amount;
      BOOST_CHECK_MESSAGE(state.CalculateWithdrawAmount(finalizer_address, amount) == +esperanza::Result::SUCCESS,
                          strprintf("test_case=%i: loop=%i: cannot calculate withdraw amount", test_idx, i));
      BOOST_CHECK_MESSAGE(amount == test_case.withdraw_amount,
                          strprintf("test_case=%i: loop=%i: amount: expected=%d received=%d",
                                    test_idx,
                                    i,
                                    test_case.withdraw_amount,
                                    amount));
      InitializeNextEpoch(state);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
