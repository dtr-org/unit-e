// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_genesis.h>
#include <blockchain/blockchain_parameters.h>
#include <hash.h>
#include <staking/stake_validator.h>
#include <test/test_unite.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

namespace {

struct Fixture {

  blockchain::Parameters parameters = blockchain::Parameters::MainNet();
  std::unique_ptr<blockchain::Behavior> b =
      blockchain::Behavior::NewFromParameters(parameters);

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
    const CBlockIndex *AtDepth(blockchain::Depth depth) override { return nullptr; }
    const CBlockIndex *AtHeight(blockchain::Height height) override { return nullptr; }
    blockchain::Depth GetDepth(const blockchain::Height) const override { return 0; }
    const CBlockIndex *GetBlockIndex(const uint256 &) const override { return nullptr; }
    const uint256 ComputeSnapshotHash() const override { return uint256(); }
    bool ProcessNewBlock(std::shared_ptr<const CBlock> pblock) override { return false; }
    boost::optional<staking::Coin> GetUTXO(const COutPoint &) const override { return boost::none; }
    ::SyncStatus GetInitialBlockDownloadStatus() const override { return sync_status; }
  };

  std::unique_ptr<staking::ActiveChain> active_chain = std::unique_ptr<staking::ActiveChain>(new ActiveChainMock());
};

}  // namespace

BOOST_AUTO_TEST_SUITE(stake_validator_tests)

BOOST_AUTO_TEST_CASE(check_kernel) {
  Fixture fixture;
  const auto stake_validator = staking::StakeValidator::New(fixture.b.get(), fixture.active_chain.get());
  const uint256 kernel;
  const auto difficulty = blockchain::GenesisBlockBuilder().Build(fixture.parameters).nBits;
  BOOST_CHECK(stake_validator->CheckKernel(1, kernel, difficulty));
}

BOOST_AUTO_TEST_CASE(check_kernel_fail) {
  Fixture fixture;
  const auto stake_validator = staking::StakeValidator::New(fixture.b.get(), fixture.active_chain.get());
  const uint256 kernel = uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
  const auto difficulty = blockchain::GenesisBlockBuilder().Build(fixture.parameters).nBits;
  BOOST_CHECK(!stake_validator->CheckKernel(1, kernel, difficulty));
}

BOOST_AUTO_TEST_CASE(remember_and_forget) {
  Fixture fixture;
  const auto stake_validator = staking::StakeValidator::New(fixture.b.get(), fixture.active_chain.get());
  const uint256 txid = uint256S("000000000000000000000000e6b8347d447e02ed383a3e96986815d576fb2a5a");
  const COutPoint stake(txid, 2);
  LOCK(stake_validator->GetLock());
  BOOST_CHECK(!stake_validator->IsPieceOfStakeKnown(stake));
  stake_validator->RememberPieceOfStake(stake);
  BOOST_CHECK(stake_validator->IsPieceOfStakeKnown(stake));
  stake_validator->ForgetPieceOfStake(stake);
  BOOST_CHECK(!stake_validator->IsPieceOfStakeKnown(stake));
}

BOOST_AUTO_TEST_SUITE_END()
