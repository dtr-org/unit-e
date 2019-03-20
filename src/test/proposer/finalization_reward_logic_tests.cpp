// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/finalization_reward_logic.h>

#include <finalization/state_repository.h>
#include <staking/validation_result.h>

#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
#include <boost/test/unit_test.hpp>

#include <functional>

namespace {

struct Fixture {
  blockchain::Parameters parameters = blockchain::Parameters::TestNet();
  std::unique_ptr<blockchain::Behavior> behavior = blockchain::Behavior::NewFromParameters(parameters);

  CBlockIndex tip;
  CBlockIndex at_depth_1;

  std::unique_ptr<finalization::StateRepository> state_repository = finalization::StateRepository::New(nullptr);

  std::unique_ptr<proposer::FinalizationRewardLogic> GetFinalizationRewardLogic() {
    return proposer::FinalizationRewardLogic::New(behavior.get(), state_repository.get());
  }
};

}  // namespace

BOOST_AUTO_TEST_SUITE(finalization_reward_logic_tests)

BOOST_AUTO_TEST_CASE(get_finalization_rewards) {
  Fixture f;
  auto logic = f.GetFinalizationRewardLogic();
}

BOOST_AUTO_TEST_SUITE_END()
