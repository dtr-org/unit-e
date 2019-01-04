// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer_logic.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

#include <functional>

namespace {

struct Fixture {

  blockchain::Parameters parameters = blockchain::Parameters::MainNet();
  std::unique_ptr<blockchain::Behavior> behavior = blockchain::Behavior::NewFromParameters(parameters);

  class NetworkMock : public staking::Network {
   public:
    int64_t GetTime() const override {}
    size_t GetNodeCount() const override {}
    size_t GetInboundNodeCount() const override {}
    size_t GetOutboundNodeCount() const override {}
  };

  class ActiveChainMock : public staking::ActiveChain {
    mutable CCriticalSection lock;

   public:
    CBlockIndex tip;
    CBlockIndex at_depth_1;
    ::SyncStatus sync_status = SyncStatus::SYNCED;

    CCriticalSection &GetLock() const override { return lock; }
    blockchain::Height GetSize() const override { return 1; }
    blockchain::Height GetHeight() const override { return 0; }
    const CBlockIndex *GetTip() const override { return &tip; }
    const CBlockIndex *AtDepth(blockchain::Depth depth) override {
      switch (depth) {
        case 1:
          return &at_depth_1;
      }
      return nullptr;
    }
    const CBlockIndex *AtHeight(blockchain::Height height) override { return nullptr; }
    const uint256 ComputeSnapshotHash() const override { return uint256(); }
    bool ProcessNewBlock(std::shared_ptr<const CBlock> pblock) override { return false; }
    ::SyncStatus GetInitialBlockDownloadStatus() const override { return sync_status; }
  };

  class StakeValidatorMock : public staking::StakeValidator {
    mutable CCriticalSection lock;

   public:
    std::function<bool(uint256)> checkkernelfunc =
        [](uint256 kernel) { return false; };
    std::function<uint256(const CBlockIndex *, const staking::Coin &, blockchain::Time)> computekernelfunc =
        [](const CBlockIndex *, const staking::Coin &, blockchain::Time) { return uint256(); };

    CCriticalSection &GetLock() override { return lock; }
    bool CheckKernel(CAmount, const uint256 &kernel, blockchain::Difficulty) const override {
      return checkkernelfunc(kernel);
    }
    uint256 ComputeKernelHash(const CBlockIndex *blockindex, const staking::Coin &coin, blockchain::Time time) const override {
      return computekernelfunc(blockindex, coin, time);
    }
    uint256 ComputeStakeModifier(const CBlockIndex *, const uint256 &) const override { return uint256(); }
    bool IsKernelKnown(const uint256 &) override { return false; }
    void RememberKernel(const uint256 &) override {}
    void ForgetKernel(const uint256 &) override {}
  };

  NetworkMock network_mock;
  ActiveChainMock active_chain_mock;
  StakeValidatorMock stake_validator_mock;

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
  const std::vector<staking::Coin> coins{
      staking::Coin{t1, 7, 20, 1},
      staking::Coin{t2, 2, 50, 1},
      staking::Coin{t3, 4, 70, 1}};
  LOCK(f.active_chain_mock.GetLock());
  const auto coin = logic->TryPropose(coins);
}

BOOST_AUTO_TEST_SUITE_END()
