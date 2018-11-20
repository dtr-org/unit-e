#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <finalization/vote_recorder.h>
#include <test/esperanza/finalizationstate_utils.h>
#include <test/test_unite.h>
#include <validationinterface.h>

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

  uint160 validatorAddress = RandValidatorAddr();
  state->ProcessDeposit(validatorAddress, 1000000);
  state->InitializeEpoch(50);
  state->InitializeEpoch(100);
  state->InitializeEpoch(150);

  // Test one single vote is added
  esperanza::Vote vote{validatorAddress, GetRandHash(), 1, 2};
  recorder->RecordVote(vote, ToByteVector(GetRandHash()));
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote.GetHash(),
                    recorder->GetVote(validatorAddress, 2)->vote.GetHash());

  // Test that a second vote does not replace the first
  esperanza::Vote vote2{validatorAddress, GetRandHash(), 2, 3};
  recorder->RecordVote(vote2, ToByteVector(GetRandHash()));
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote2.GetHash(),
                    recorder->GetVote(validatorAddress, 3)->vote.GetHash());

  // Test that the same vote could be registered multiple times
  recorder->RecordVote(vote2, ToByteVector(GetRandHash()));
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote2.GetHash(),
                    recorder->GetVote(validatorAddress, 3)->vote.GetHash());
}

BOOST_AUTO_TEST_CASE(record_double_vote) {

  auto state = FinalizationState::GetState();

  SlashListener listener;
  RegisterValidationInterface(&listener);
  auto recorder = VoteRecorder::GetVoteRecorder();

  uint160 validatorAddress = RandValidatorAddr();
  state->ProcessDeposit(validatorAddress, 1000000);
  state->InitializeEpoch(50);
  state->InitializeEpoch(100);
  state->InitializeEpoch(150);

  esperanza::Vote vote1{validatorAddress, GetRandHash(), 5, 10};
  esperanza::Vote vote2{validatorAddress, GetRandHash(), 7, 10};

  recorder->RecordVote(vote1, ToByteVector(GetRandHash()));
  BOOST_CHECK(!listener.slashingDetected);
  BOOST_CHECK_EQUAL(vote1.GetHash(),
                    recorder->GetVote(validatorAddress, 10)->vote.GetHash());

  recorder->RecordVote(vote2, ToByteVector(GetRandHash()));
  // Duplicate votes are not inserted
  BOOST_CHECK_EQUAL(vote1.GetHash(),
                    recorder->GetVote(validatorAddress, 10)->vote.GetHash());
  BOOST_CHECK(listener.slashingDetected);
}

BOOST_AUTO_TEST_CASE(record_surrounding_vote_inner_passed) {

  FinalizationState::Reset(FinalizationParams(), AdminParams());
  auto state = FinalizationState::GetState();

  SlashListener listener;
  RegisterValidationInterface(&listener);
  auto recorder = VoteRecorder::GetVoteRecorder();

  uint160 validatorAddress = RandValidatorAddr();
  state->ProcessDeposit(validatorAddress, 1000000);
  state->InitializeEpoch(50);
  state->InitializeEpoch(100);
  state->InitializeEpoch(150);

  esperanza::Vote outerVote{validatorAddress, GetRandHash(), 1, 10};
  esperanza::Vote innerVote{validatorAddress, GetRandHash(), 2, 9};

  recorder->RecordVote(outerVote, ToByteVector(GetRandHash()));
  BOOST_CHECK_EQUAL(outerVote.GetHash(),
                    recorder->GetVote(validatorAddress, 10)->vote.GetHash());
  BOOST_CHECK(!listener.slashingDetected);

  recorder->RecordVote(innerVote, ToByteVector(GetRandHash()));
  BOOST_CHECK_EQUAL(innerVote.GetHash(),
                    recorder->GetVote(validatorAddress, 9)->vote.GetHash());
  BOOST_CHECK(listener.slashingDetected);
}

BOOST_AUTO_TEST_CASE(record_surrounding_vote_outer_passed) {
  FinalizationState::Reset(FinalizationParams(), AdminParams());
  auto state = FinalizationState::GetState();

  SlashListener listener;
  RegisterValidationInterface(&listener);
  auto recorder = VoteRecorder::GetVoteRecorder();

  uint160 validatorAddress = RandValidatorAddr();
  state->ProcessDeposit(validatorAddress, 1000000);
  state->InitializeEpoch(50);
  state->InitializeEpoch(100);
  state->InitializeEpoch(150);

  esperanza::Vote outerVote{validatorAddress, GetRandHash(), 1, 10};
  esperanza::Vote innerVote{validatorAddress, GetRandHash(), 2, 9};

  recorder->RecordVote(innerVote, ToByteVector(GetRandHash()));
  BOOST_CHECK_EQUAL(innerVote.GetHash(),
                    recorder->GetVote(validatorAddress, 9)->vote.GetHash());
  BOOST_CHECK(!listener.slashingDetected);

  recorder->RecordVote(outerVote, ToByteVector(GetRandHash()));
  BOOST_CHECK_EQUAL(outerVote.GetHash(),
                    recorder->GetVote(validatorAddress, 10)->vote.GetHash());
  BOOST_CHECK(listener.slashingDetected);
}

BOOST_AUTO_TEST_SUITE_END()
