// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/esperanza/finalizationstate_utils.h>

BOOST_FIXTURE_TEST_SUITE(finalizationstate_calculate_withdraw_amount_tests, ReducedTestingSetup)

void CreateAndProcessVote(FinalizationStateSpy &state, uint160 finalizer_address, uint256 target_hash) {
  Vote vote{finalizer_address, target_hash, state.GetExpectedSourceEpoch(), state.GetRecommendedTargetEpoch()};
  BOOST_REQUIRE_EQUAL(state.ValidateVote(vote), +Result::SUCCESS);
  state.ProcessVote(vote);
}

void InitializeNextEpoch(FinalizationStateSpy &state) {
  state.SetRecommendedTargetEpoch(state.GetCurrentEpoch());

  Result res = state.InitializeEpoch(1 + state.GetCurrentEpoch() * state.EpochLength());
  BOOST_REQUIRE_EQUAL(res, +Result::SUCCESS);
}

BOOST_AUTO_TEST_CASE(calculate_withdraw_amount_always_voting) {
  // This test creates one finalizer which always votes and justifies epochs.
  // Each test case configures how many epochs the finalizer votes before logout
  // and asserts the withdrawal amount.
  struct TestCase {
    std::string comment;
    uint32_t epochs_before_logout;
    CAmount deposit_amount;
    CAmount withdraw_amount;
  };

  std::vector<TestCase> test_cases{
      TestCase{
          "logout right away",
          0,
          1000000000000,
          1000011069779,
      },
      TestCase{
          "vote once and logout",
          1,
          1000000000000,
          1000014759724,
      },
      TestCase{
          "vote 10 times and logout",
          10,
          1000000000000,
          1000047969221,
      },
      TestCase{
          "vote 100 times and logout",
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
    BOOST_REQUIRE_EQUAL(state.GetLastFinalizedEpoch(), state.GetCurrentEpoch() - 1);  // last one insta-justified

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

BOOST_AUTO_TEST_CASE(calculate_withdraw_amount_sometimes_voting) {
  // This test creates the `finalizer_address` finalizer which logouts after
  // `epochs_before_logout` epochs and votes in first `vote_in_epochs` epochs.
  // However, in every epoch finalization is reached as there is a second
  // `large_finalizer_address` finalizer that holds the majority of deposits.
  struct TestCase {
    std::string comment;
    uint32_t epochs_before_logout;
    uint32_t vote_in_epochs;
    CAmount deposit_amount;
    CAmount withdraw_amount;
  };

  std::vector<TestCase> test_cases{
      TestCase{
          "logout after 50 epochs. Don't vote",
          50,
          0,
          1000000000000,
          999868240000,
      },
      TestCase{
          "logout after 50 epochs. Vote in first 10 epochs",
          50,
          10,
          1000000000000,
          999903276441,
      },
      TestCase{
          "logout after 50 epochs. Vote in first 20 epochs",
          50,
          20,
          1000000000000,
          999947073698,
      },
      TestCase{
          "logout after 50 epochs. Vote in first 30 epochs",
          50,
          30,
          1000000000000,
          999990872849,
      },
      TestCase{
          "logout after 50 epochs. Vote in first 40 epochs",
          50,
          40,
          1000000000000,
          1000034673893,
      },
      TestCase{
          "logout after 50 epochs. Vote in all epochs",
          50,
          50,
          1000000000000,
          1000078476834,
      },
  };

  for (size_t test_idx = 0; test_idx < test_cases.size(); ++test_idx) {
    TestCase test_case = test_cases[test_idx];
    BOOST_REQUIRE(test_case.epochs_before_logout >= test_case.vote_in_epochs);

    // setup
    finalization::Params params = finalization::Params::TestNet();
    FinalizationStateSpy state(params);

    // mock target hash
    uint256 target_hash = GetRandHash();
    CBlockIndex block_index;
    block_index.phashBlock = &target_hash;
    state.SetRecommendedTarget(block_index);

    // deposit
    uint160 large_finalizer_address = RandValidatorAddr();
    state.CreateDeposit(large_finalizer_address, test_case.deposit_amount * 3);

    uint160 finalizer_address = RandValidatorAddr();
    state.CreateAndActivateDeposit(finalizer_address, test_case.deposit_amount);
    BOOST_REQUIRE_EQUAL(state.GetActiveFinalizers().size(), 2);

    // vote before logout
    uint32_t end = state.GetCurrentEpoch() + test_case.epochs_before_logout;
    uint32_t vote_until = state.GetCurrentEpoch() + test_case.vote_in_epochs;
    for (uint32_t i = state.GetCurrentEpoch(); i < end; ++i) {
      if (i < vote_until) {
        CreateAndProcessVote(state, finalizer_address, target_hash);
      }

      CreateAndProcessVote(state, large_finalizer_address, target_hash);
      InitializeNextEpoch(state);
    }

    // logout
    BOOST_REQUIRE_EQUAL(state.ValidateLogout(finalizer_address), +Result::SUCCESS);
    state.ProcessLogout(finalizer_address);
    BOOST_REQUIRE_EQUAL(state.GetCurrentEpoch(), 4 + test_case.epochs_before_logout);
    BOOST_REQUIRE_EQUAL(state.GetCurrentDynasty(), 2 + test_case.epochs_before_logout);

    // pass logout delay
    uint32_t end_logout = state.GetCurrentEpoch() + static_cast<uint32_t>(state.DynastyLogoutDelay());
    BOOST_REQUIRE_EQUAL(end_logout, 9 + test_case.epochs_before_logout);

    const esperanza::Validator *finalizer = state.GetValidator(finalizer_address);
    BOOST_REQUIRE(finalizer);

    for (uint32_t i = state.GetCurrentEpoch(); i <= end_logout; ++i) {
      CreateAndProcessVote(state, large_finalizer_address, target_hash);
      InitializeNextEpoch(state);
      BOOST_REQUIRE_EQUAL(state.GetLastFinalizedEpoch(), state.GetCurrentEpoch() - 2);
    }

    // wait withdraw delay
    uint32_t end_withdraw = end_logout + 1 + static_cast<uint32_t>(state.WithdrawalEpochDelay());
    BOOST_REQUIRE_EQUAL(end_withdraw, 20 + test_case.epochs_before_logout);

    for (uint32_t i = state.GetCurrentEpoch(); i < end_withdraw; ++i) {
      CreateAndProcessVote(state, large_finalizer_address, target_hash);

      BOOST_REQUIRE_EQUAL(state.ValidateWithdraw(finalizer_address, test_case.deposit_amount), +Result::WITHDRAW_TOO_EARLY);
      CAmount amount;
      BOOST_REQUIRE_EQUAL(state.CalculateWithdrawAmount(finalizer_address, amount), +Result::WITHDRAW_TOO_EARLY);
      InitializeNextEpoch(state);
      BOOST_REQUIRE_EQUAL(state.GetLastFinalizedEpoch(), state.GetCurrentEpoch() - 2);
    }

    // test amount
    BOOST_REQUIRE_EQUAL(state.ValidateWithdraw(finalizer_address, test_case.deposit_amount), +Result::SUCCESS);

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
