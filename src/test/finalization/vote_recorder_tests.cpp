// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/vote_recorder.h>
#include <test/esperanza/finalizationstate_utils.h>
#include <test/test_unite.h>
#include <validationinterface.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

BOOST_FIXTURE_TEST_SUITE(vote_recorder_tests, TestingSetup)

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

BOOST_AUTO_TEST_CASE(singleton) {

  VoteRecorder::Init();
  std::shared_ptr<VoteRecorder> instance = VoteRecorder::GetVoteRecorder();

  VoteRecorder::Init();
  std::shared_ptr<VoteRecorder> instance2 = VoteRecorder::GetVoteRecorder();

  BOOST_CHECK_EQUAL(instance.get(), instance2.get());
}

BOOST_AUTO_TEST_CASE(record_votes) {

  auto state = FinalizationState::GetState();

  SlashListener listener;
  RegisterValidationInterface(&listener);
  auto recorder = VoteRecorder::GetVoteRecorder();

  uint160 validator_address = RandValidatorAddr();
  state->ProcessDeposit(validator_address, 1000000);
  state->InitializeEpoch(1);
  state->InitializeEpoch(1 + 1 * 50);
  state->InitializeEpoch(1 + 2 * 50);
  state->InitializeEpoch(1 + 3 * 50);
  state->InitializeEpoch(1 + 4 * 50);

  // Test one single vote is added
  esperanza::Vote vote{validator_address, GetRandHash(), 1, 2};
  recorder->RecordVote(vote, ToByteVector(GetRandHash()));
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote.GetHash(),
                    recorder->GetVote(validator_address, 2)->vote.GetHash());

  // Test that a second vote does not replace the first
  esperanza::Vote vote2{validator_address, GetRandHash(), 2, 3};
  recorder->RecordVote(vote2, ToByteVector(GetRandHash()));
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote2.GetHash(),
                    recorder->GetVote(validator_address, 3)->vote.GetHash());

  // Test that the same vote could be registered multiple times
  recorder->RecordVote(vote2, ToByteVector(GetRandHash()));
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote2.GetHash(),
                    recorder->GetVote(validator_address, 3)->vote.GetHash());

  // Test that almost surrounding votes are not detected as slashable
  esperanza::Vote outer_vote{validator_address, GetRandHash(), 3, 10};
  esperanza::Vote inner_vote{validator_address, GetRandHash(), 3, 9};

  recorder->RecordVote(outer_vote, ToByteVector(GetRandHash()));
  BOOST_CHECK_EQUAL(outer_vote.GetHash(),
                    recorder->GetVote(validator_address, 10)->vote.GetHash());
  BOOST_CHECK(!listener.slashingDetected);

  recorder->RecordVote(inner_vote, ToByteVector(GetRandHash()));
  BOOST_CHECK_EQUAL(inner_vote.GetHash(),
                    recorder->GetVote(validator_address, 9)->vote.GetHash());
  BOOST_CHECK(!listener.slashingDetected);

  UnregisterValidationInterface(&listener);
}

BOOST_AUTO_TEST_CASE(record_double_vote) {

  auto state = FinalizationState::GetState();

  SlashListener listener;
  RegisterValidationInterface(&listener);
  auto recorder = VoteRecorder::GetVoteRecorder();

  uint160 validator_address = RandValidatorAddr();
  state->ProcessDeposit(validator_address, 1000000);
  state->InitializeEpoch(1);
  state->InitializeEpoch(1 + 1 * 50);
  state->InitializeEpoch(1 + 2 * 50);
  state->InitializeEpoch(1 + 3 * 50);
  state->InitializeEpoch(1 + 4 * 50);

  esperanza::Vote vote1{validator_address, GetRandHash(), 5, 10};
  esperanza::Vote vote2{validator_address, GetRandHash(), 7, 10};

  recorder->RecordVote(vote1, ToByteVector(GetRandHash()));
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote1.GetHash(),
                    recorder->GetVote(validator_address, 10)->vote.GetHash());

  recorder->RecordVote(vote2, ToByteVector(GetRandHash()));
  // Duplicate votes are not inserted
  BOOST_CHECK_EQUAL(vote1.GetHash(),
                    recorder->GetVote(validator_address, 10)->vote.GetHash());
  BOOST_CHECK(listener.slashingDetected);

  UnregisterValidationInterface(&listener);
}

BOOST_AUTO_TEST_CASE(record_surrounding_vote_inner_passed) {

  auto state = FinalizationState::GetState();

  SlashListener listener;
  RegisterValidationInterface(&listener);
  auto recorder = VoteRecorder::GetVoteRecorder();

  uint160 validator_address = RandValidatorAddr();
  state->ProcessDeposit(validator_address, 1000000);
  state->InitializeEpoch(1);
  state->InitializeEpoch(1 + 1 * 50);
  state->InitializeEpoch(1 + 2 * 50);
  state->InitializeEpoch(1 + 3 * 50);
  state->InitializeEpoch(1 + 4 * 50);

  esperanza::Vote outer_vote{validator_address, GetRandHash(), 1, 10};
  esperanza::Vote inner_vote{validator_address, GetRandHash(), 2, 9};

  recorder->RecordVote(outer_vote, ToByteVector(GetRandHash()));
  BOOST_CHECK_EQUAL(outer_vote.GetHash(),
                    recorder->GetVote(validator_address, 10)->vote.GetHash());
  BOOST_CHECK(!listener.slashingDetected);

  recorder->RecordVote(inner_vote, ToByteVector(GetRandHash()));
  BOOST_CHECK_EQUAL(inner_vote.GetHash(),
                    recorder->GetVote(validator_address, 9)->vote.GetHash());
  BOOST_CHECK(listener.slashingDetected);

  UnregisterValidationInterface(&listener);
}

BOOST_AUTO_TEST_CASE(record_surrounding_vote_outer_passed) {

  auto state = FinalizationState::GetState();

  SlashListener listener;
  RegisterValidationInterface(&listener);
  auto recorder = VoteRecorder::GetVoteRecorder();

  uint160 validator_address = RandValidatorAddr();
  state->ProcessDeposit(validator_address, 1000000);
  state->InitializeEpoch(1);
  state->InitializeEpoch(1 + 1 * 50);
  state->InitializeEpoch(1 + 2 * 50);
  state->InitializeEpoch(1 + 3 * 50);
  state->InitializeEpoch(1 + 4 * 50);

  esperanza::Vote outer_vote{validator_address, GetRandHash(), 1, 10};
  esperanza::Vote inner_vote{validator_address, GetRandHash(), 2, 9};

  recorder->RecordVote(inner_vote, ToByteVector(GetRandHash()));
  BOOST_CHECK_EQUAL(inner_vote.GetHash(),
                    recorder->GetVote(validator_address, 9)->vote.GetHash());
  BOOST_CHECK(!listener.slashingDetected);

  recorder->RecordVote(outer_vote, ToByteVector(GetRandHash()));
  BOOST_CHECK_EQUAL(outer_vote.GetHash(),
                    recorder->GetVote(validator_address, 10)->vote.GetHash());
  BOOST_CHECK(listener.slashingDetected);

  UnregisterValidationInterface(&listener);
}

BOOST_AUTO_TEST_SUITE_END()
