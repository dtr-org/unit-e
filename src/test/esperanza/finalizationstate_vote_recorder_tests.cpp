// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/finalizationstate.h>
#include <finalization/vote_recorder.h>
#include <test/esperanza/finalizationstate_utils.h>
#include <test/test_unite.h>
#include <tinyformat.h>
#include <validationinterface.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

BOOST_FIXTURE_TEST_SUITE(finalizationstate_vote_recorder_tests, TestingSetup)

using namespace finalization;

class SlashListener : public CValidationInterface {

 public:
  bool slashingDetected = false;

 protected:
  void SlashingConditionDetected(const finalization::VoteRecord &,
                                 const finalization::VoteRecord &) override {
    slashingDetected = true;
  }
};

BOOST_AUTO_TEST_CASE(record_double_vote) {
  // initialize
  FinalizationStateSpy spy;
  SlashListener listener;
  RegisterValidationInterface(&listener);

  // record first vote that the double vote will be checked against it
  uint160 finalizer_address = RandValidatorAddr();
  spy.CreateAndActivateDeposit(finalizer_address, spy.MinDepositSize());
  esperanza::Vote first_vote{finalizer_address, GetRandHash(), 5, 10};
  VoteRecorder::GetVoteRecorder()->RecordVote(first_vote, ToByteVector(GetRandHash()));

  struct TestCase {
    Result vote_validation_result;
    bool slashing_detected;
  };

  std::vector<TestCase> test_cases{
      TestCase{Result::SUCCESS, true},
      TestCase{Result::VOTE_ALREADY_VOTED, true},
      TestCase{Result::VOTE_WRONG_TARGET_HASH, true},
      TestCase{Result::VOTE_WRONG_TARGET_EPOCH, true},
      TestCase{Result::VOTE_SRC_EPOCH_NOT_JUSTIFIED, true},

      TestCase{Result::VOTE_NOT_VOTABLE, false},
      TestCase{Result::INIT_WRONG_EPOCH, false},
      TestCase{Result::VOTE_NOT_BY_VALIDATOR, false},
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    listener.slashingDetected = false;

    TestCase tc = test_cases[i];
    esperanza::Vote double_vote{finalizer_address, GetRandHash(), 6, 10};
    spy.RecordVoteIfNeeded(tc.vote_validation_result, double_vote, ToByteVector(GetRandHash()));

    BOOST_CHECK_MESSAGE(listener.slashingDetected == tc.slashing_detected,
                        strprintf("test_case=%d failed", i));
  }

  UnregisterValidationInterface(&listener);
}

BOOST_AUTO_TEST_CASE(record_surrounding_vote_inner_passed) {
  // initialize
  FinalizationStateSpy spy;
  SlashListener listener;
  RegisterValidationInterface(&listener);

  // record first vote that the double vote will be checked against it
  uint160 finalizer_address = RandValidatorAddr();
  spy.CreateAndActivateDeposit(finalizer_address, spy.MinDepositSize());
  esperanza::Vote first_vote{finalizer_address, GetRandHash(), 5, 10};
  VoteRecorder::GetVoteRecorder()->RecordVote(first_vote, ToByteVector(GetRandHash()));

  struct TestCase {
    Result vote_validation_result;
    bool slashing_detected;
  };

  std::vector<TestCase> test_cases{
      TestCase{Result::SUCCESS, true},
      TestCase{Result::VOTE_ALREADY_VOTED, true},
      TestCase{Result::VOTE_WRONG_TARGET_HASH, true},
      TestCase{Result::VOTE_WRONG_TARGET_EPOCH, true},
      TestCase{Result::VOTE_SRC_EPOCH_NOT_JUSTIFIED, true},

      TestCase{Result::VOTE_NOT_VOTABLE, false},
      TestCase{Result::INIT_WRONG_EPOCH, false},
      TestCase{Result::VOTE_NOT_BY_VALIDATOR, false},
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    listener.slashingDetected = false;

    TestCase tc = test_cases[i];
    esperanza::Vote double_vote{finalizer_address, GetRandHash(), 6, 9};
    spy.RecordVoteIfNeeded(tc.vote_validation_result, double_vote, ToByteVector(GetRandHash()));

    BOOST_CHECK_MESSAGE(listener.slashingDetected == tc.slashing_detected,
                        strprintf("test_case=%d failed", i));
  }

  UnregisterValidationInterface(&listener);
}

BOOST_AUTO_TEST_CASE(record_surrounding_vote_outer_passed) {
  // initialize
  FinalizationStateSpy spy;
  SlashListener listener;
  RegisterValidationInterface(&listener);

  // record first vote that the double vote will be checked against it
  uint160 finalizer_address = RandValidatorAddr();
  spy.CreateAndActivateDeposit(finalizer_address, spy.MinDepositSize());
  esperanza::Vote first_vote{finalizer_address, GetRandHash(), 5, 10};
  VoteRecorder::GetVoteRecorder()->RecordVote(first_vote, ToByteVector(GetRandHash()));

  struct TestCase {
    Result vote_validation_result;
    bool slashing_detected;
  };

  std::vector<TestCase> test_cases{
      TestCase{Result::SUCCESS, true},
      TestCase{Result::VOTE_ALREADY_VOTED, true},
      TestCase{Result::VOTE_WRONG_TARGET_HASH, true},
      TestCase{Result::VOTE_WRONG_TARGET_EPOCH, true},
      TestCase{Result::VOTE_SRC_EPOCH_NOT_JUSTIFIED, true},

      TestCase{Result::VOTE_NOT_VOTABLE, false},
      TestCase{Result::INIT_WRONG_EPOCH, false},
      TestCase{Result::VOTE_NOT_BY_VALIDATOR, false},
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    listener.slashingDetected = false;

    TestCase tc = test_cases[i];
    esperanza::Vote double_vote{finalizer_address, GetRandHash(), 4, 11};
    spy.RecordVoteIfNeeded(tc.vote_validation_result, double_vote, ToByteVector(GetRandHash()));

    BOOST_CHECK_MESSAGE(listener.slashingDetected == tc.slashing_detected,
                        strprintf("test_case=%d failed", i));
  }

  UnregisterValidationInterface(&listener);
}

BOOST_AUTO_TEST_SUITE_END()
