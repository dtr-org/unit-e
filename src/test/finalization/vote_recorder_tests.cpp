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

  VoteRecorder::DBParams params;
  params.inmemory = true;
  VoteRecorder::Init(params);
  std::shared_ptr<VoteRecorder> instance = VoteRecorder::GetVoteRecorder();

  VoteRecorder::Init(params);
  std::shared_ptr<VoteRecorder> instance2 = VoteRecorder::GetVoteRecorder();

  BOOST_CHECK_EQUAL(instance.get(), instance2.get());
}

BOOST_AUTO_TEST_CASE(record_votes) {

  finalization::Params params = finalization::Params::TestNet();
  FinalizationStateSpy spy(params);
  SlashListener listener;
  RegisterValidationInterface(&listener);
  auto recorder = VoteRecorder::GetVoteRecorder();

  uint160 validatorAddress = RandValidatorAddr();
  spy.ProcessDeposit(validatorAddress, 1000000);
  spy.InitializeEpoch(1);
  spy.InitializeEpoch(1 + 1 * 50);
  spy.InitializeEpoch(1 + 2 * 50);
  spy.InitializeEpoch(1 + 3 * 50);
  spy.InitializeEpoch(1 + 4 * 50);
  spy.InitializeEpoch(1 + 5 * 50);

  // Test one single vote is added
  esperanza::Vote vote{validatorAddress, GetRandHash(), 1, 2};
  recorder->RecordVote(vote, ToByteVector(GetRandHash()), spy);
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote.GetHash(),
                    recorder->GetVote(validatorAddress, 2)->vote.GetHash());

  // Test that a second vote does not replace the first
  esperanza::Vote vote2{validatorAddress, GetRandHash(), 2, 3};
  recorder->RecordVote(vote2, ToByteVector(GetRandHash()), spy);
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote2.GetHash(),
                    recorder->GetVote(validatorAddress, 3)->vote.GetHash());

  // Test that the same vote could be registered multiple times
  recorder->RecordVote(vote2, ToByteVector(GetRandHash()), spy);
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote2.GetHash(),
                    recorder->GetVote(validatorAddress, 3)->vote.GetHash());

  // Test that almost surrounding votes are not detected as slashable
  esperanza::Vote outerVote{validatorAddress, GetRandHash(), 3, 10};
  esperanza::Vote innerVote{validatorAddress, GetRandHash(), 3, 9};

  recorder->RecordVote(outerVote, ToByteVector(GetRandHash()), spy);
  BOOST_CHECK_EQUAL(outerVote.GetHash(),
                    recorder->GetVote(validatorAddress, 10)->vote.GetHash());
  BOOST_CHECK(!listener.slashingDetected);

  recorder->RecordVote(innerVote, ToByteVector(GetRandHash()), spy);
  BOOST_CHECK_EQUAL(innerVote.GetHash(),
                    recorder->GetVote(validatorAddress, 9)->vote.GetHash());
  BOOST_CHECK(!listener.slashingDetected);

  UnregisterValidationInterface(&listener);
}

BOOST_AUTO_TEST_CASE(record_double_vote) {

  finalization::Params params = finalization::Params::TestNet();
  FinalizationStateSpy spy(params);
  SlashListener listener;
  RegisterValidationInterface(&listener);
  auto recorder = VoteRecorder::GetVoteRecorder();

  uint160 validatorAddress = RandValidatorAddr();
  spy.ProcessDeposit(validatorAddress, 1000000);
  spy.InitializeEpoch(1);
  spy.InitializeEpoch(1 + 1 * 50);
  spy.InitializeEpoch(1 + 2 * 50);
  spy.InitializeEpoch(1 + 3 * 50);
  spy.InitializeEpoch(1 + 4 * 50);
  spy.InitializeEpoch(1 + 5 * 50);

  esperanza::Vote vote1{validatorAddress, GetRandHash(), 5, 10};
  esperanza::Vote vote2{validatorAddress, GetRandHash(), 7, 10};

  recorder->RecordVote(vote1, ToByteVector(GetRandHash()), spy);
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote1.GetHash(),
                    recorder->GetVote(validatorAddress, 10)->vote.GetHash());

  recorder->RecordVote(vote2, ToByteVector(GetRandHash()), spy);
  // Duplicate votes are not inserted
  BOOST_CHECK_EQUAL(vote1.GetHash(),
                    recorder->GetVote(validatorAddress, 10)->vote.GetHash());
  BOOST_CHECK(listener.slashingDetected);

  UnregisterValidationInterface(&listener);
}

BOOST_AUTO_TEST_CASE(record_surrounding_vote_inner_passed) {

  finalization::Params params = finalization::Params::TestNet();
  FinalizationStateSpy spy(params);
  SlashListener listener;
  RegisterValidationInterface(&listener);
  auto recorder = VoteRecorder::GetVoteRecorder();

  uint160 validatorAddress = RandValidatorAddr();
  spy.ProcessDeposit(validatorAddress, 1000000);
  spy.InitializeEpoch(1);
  spy.InitializeEpoch(1 + 1 * 50);
  spy.InitializeEpoch(1 + 2 * 50);
  spy.InitializeEpoch(1 + 3 * 50);
  spy.InitializeEpoch(1 + 4 * 50);
  spy.InitializeEpoch(1 + 5 * 50);

  esperanza::Vote outerVote{validatorAddress, GetRandHash(), 1, 10};
  esperanza::Vote innerVote{validatorAddress, GetRandHash(), 2, 9};

  recorder->RecordVote(outerVote, ToByteVector(GetRandHash()), spy);
  BOOST_CHECK_EQUAL(outerVote.GetHash(),
                    recorder->GetVote(validatorAddress, 10)->vote.GetHash());
  BOOST_CHECK(!listener.slashingDetected);

  recorder->RecordVote(innerVote, ToByteVector(GetRandHash()), spy);
  BOOST_CHECK_EQUAL(innerVote.GetHash(),
                    recorder->GetVote(validatorAddress, 9)->vote.GetHash());
  BOOST_CHECK(listener.slashingDetected);

  UnregisterValidationInterface(&listener);
}

BOOST_AUTO_TEST_CASE(record_surrounding_vote_outer_passed) {

  finalization::Params params = finalization::Params::TestNet();
  FinalizationStateSpy spy(params);

  SlashListener listener;
  RegisterValidationInterface(&listener);
  auto recorder = VoteRecorder::GetVoteRecorder();

  uint160 validatorAddress = RandValidatorAddr();
  spy.ProcessDeposit(validatorAddress, 1000000);
  spy.InitializeEpoch(1);
  spy.InitializeEpoch(1 + 1 * 50);
  spy.InitializeEpoch(1 + 2 * 50);
  spy.InitializeEpoch(1 + 3 * 50);
  spy.InitializeEpoch(1 + 4 * 50);
  spy.InitializeEpoch(1 + 5 * 50);

  esperanza::Vote outerVote{validatorAddress, GetRandHash(), 1, 10};
  esperanza::Vote innerVote{validatorAddress, GetRandHash(), 2, 9};

  recorder->RecordVote(innerVote, ToByteVector(GetRandHash()), spy);
  BOOST_CHECK_EQUAL(innerVote.GetHash(),
                    recorder->GetVote(validatorAddress, 9)->vote.GetHash());
  BOOST_CHECK(!listener.slashingDetected);

  recorder->RecordVote(outerVote, ToByteVector(GetRandHash()), spy);
  BOOST_CHECK_EQUAL(outerVote.GetHash(),
                    recorder->GetVote(validatorAddress, 10)->vote.GetHash());
  BOOST_CHECK(listener.slashingDetected);

  UnregisterValidationInterface(&listener);
}

BOOST_AUTO_TEST_SUITE_END()
