// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer_logic.h>

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

  mocks::NetworkMock network_mock;
  mocks::ActiveChainMock active_chain_mock;
  mocks::StakeValidatorMock stake_validator_mock;

  std::unique_ptr<proposer::Logic> GetProposerLogic() {
    return proposer::Logic::New(behavior.get(), &network_mock, &active_chain_mock, &stake_validator_mock);
  }
};

}  // namespace

BOOST_AUTO_TEST_SUITE(proposer_logic_tests)

BOOST_AUTO_TEST_CASE(propose) {
  Fixture f;
  auto logic = f.GetProposerLogic();
  const uint256 t1 = uint256S("01c22ebd3beef7e855b99aa6fb203e1055656c39f3c139cd4e4b13be22ee009a");
  const uint256 t2 = uint256S("8551913c6386e7771f33e31d2e77c4941e70174a96079cd591ff823fee986e48");
  const uint256 t3 = uint256S("c6c9b8db7a83122b6680ce762d6215f1eab2bd5825cfeef903de70cbf41a9803");
  const uint256 k1 = uint256S("622cb3371c1a08096eaac564fb59acccda1fcdbe13a9dd10b486e6463c8c2525");
  const uint256 k2 = uint256S("de2157f24915d2fb7e8bb62cfc8adc81029a7b7909e503b79aac0900195d1f5c");
  const uint256 k3 = uint256S("6738ef1b0509836ea7a0fcc2f31887930454c96bb9c7bf2f6b04adbe2bb0d290");
  const CBlockIndex block = [] {
    CBlockIndex index;
    index.nHeight = 92345;
    return index;
  }();
  const staking::CoinSet coins = [&] {
    staking::CoinSet coins;
    coins.emplace(&block, COutPoint{t1, 7}, CTxOut{20, CScript()});
    coins.emplace(&block, COutPoint{t2, 2}, CTxOut{50, CScript()});
    coins.emplace(&block, COutPoint{t3, 4}, CTxOut{70, CScript()});
    return coins;
  }();
  f.active_chain_mock.result_GetTip = &f.tip;
  f.active_chain_mock.stub_AtDepth = [&f](const blockchain::Depth depth) -> CBlockIndex * {
    if (depth == 1) {
      return &f.at_depth_1;
    }
    return nullptr;
  };
  f.stake_validator_mock.checkkernelfunc = [&](uint256 kernel) {
    return kernel == k2;
  };
  f.stake_validator_mock.computekernelfunc = [&](const CBlockIndex *, const staking::Coin &coin, blockchain::Time) {
    if (coin.GetTransactionId() == t1) {
      return k1;
    }
    if (coin.GetTransactionId() == t2) {
      return k2;
    }
    if (coin.GetTransactionId() == t3) {
      return k3;
    }
    return coin.GetTransactionId();
  };
  const boost::optional<proposer::EligibleCoin> coin = [&] {
    LOCK(f.active_chain_mock.GetLock());
    return logic->TryPropose(coins);
  }();
  BOOST_REQUIRE(static_cast<bool>(coin));
  const proposer::EligibleCoin eligible_coin = *coin;
  BOOST_CHECK_EQUAL(eligible_coin.kernel_hash, k2);
  BOOST_CHECK_EQUAL(eligible_coin.utxo.GetTransactionId(), t2);
}

BOOST_AUTO_TEST_SUITE_END()
