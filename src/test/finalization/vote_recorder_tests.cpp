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
  VoteRecorder::DBParams params;
  params.inmemory = true;
  VoteRecorder::Init(params);

  std::shared_ptr<VoteRecorder> vote_recorder = VoteRecorder::GetVoteRecorder();
  BOOST_CHECK_EQUAL(vote_recorder->Count(), 0);

  Vote vote1{RandValidatorAddr(), uint256(), 5, 4};
  vote_recorder->RecordVote(vote1, std::vector<unsigned char>());
  BOOST_CHECK_EQUAL(vote_recorder->Count(), 1);

  Vote vote2{RandValidatorAddr(), uint256(), 5, 4};
  vote_recorder->RecordVote(vote2, std::vector<unsigned char>());
  BOOST_CHECK_EQUAL(vote_recorder->Count(), 2);

  // skip the same vote
  vote_recorder->RecordVote(vote2, std::vector<unsigned char>());
  BOOST_CHECK_EQUAL(vote_recorder->Count(), 2);

  // new vote of known finalizer
  Vote vote3{vote2.m_validator_address, uint256(), 6, 5};
  vote_recorder->RecordVote(vote3, std::vector<unsigned char>());
  BOOST_CHECK_EQUAL(vote_recorder->Count(), 3);
}

BOOST_AUTO_TEST_CASE(find_offending_vote) {
  VoteRecorder::DBParams params;
  params.inmemory = true;
  VoteRecorder::Init(params);

  std::shared_ptr<VoteRecorder> vote_recorder = VoteRecorder::GetVoteRecorder();
  Vote vote{RandValidatorAddr(), uint256S("aa"), 5, 10};
  vote_recorder->RecordVote(vote, std::vector<unsigned char>());
  BOOST_REQUIRE_EQUAL(vote_recorder->Count(), 1);

  struct TestCase {
    std::string test_name;
    uint160 finalizer_address;
    uint32_t source_epoch;
    uint32_t target_epoch;
    bool is_offending_vote;
  };

  std::vector<TestCase> test_cases{
      TestCase{
          "same vote but different finalizer",
          RandValidatorAddr(),
          vote.m_source_epoch,
          vote.m_target_epoch,
          false,
      },
      TestCase{
          "same source but larger target",
          vote.m_validator_address,
          vote.m_source_epoch,
          vote.m_target_epoch + 1,
          false,
      },
      TestCase{
          "same source but smaller target",
          vote.m_validator_address,
          vote.m_source_epoch,
          vote.m_target_epoch - 1,
          false,
      },
      TestCase{
          "double vote",
          vote.m_validator_address,
          vote.m_source_epoch + 1,
          vote.m_target_epoch,
          true,
      },
      TestCase{
          "surrounded inner vote",
          vote.m_validator_address,
          vote.m_source_epoch + 1,
          vote.m_target_epoch - 1,
          true,
      },
      TestCase{
          "surrounded outer vote",
          vote.m_validator_address,
          vote.m_source_epoch - 1,
          vote.m_target_epoch + 1,
          true,
      },
  };

  for (const TestCase &tc : test_cases) {
    Vote test_vote{tc.finalizer_address, uint256S("bb"), tc.source_epoch, tc.target_epoch};
    boost::optional<VoteRecord> record = vote_recorder->FindOffendingVote(test_vote);
    BOOST_CHECK_MESSAGE(!!record == tc.is_offending_vote,
                        strprintf("test: %s", tc.test_name));
  }
}

BOOST_AUTO_TEST_SUITE_END()
