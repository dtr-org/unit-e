// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/block_builder.h>

#include <blockchain/blockchain_behavior.h>
#include <primitives/transaction.h>
#include <staking/block_validator.h>
#include <wallet/wallet.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

#include <functional>

namespace {

struct Fixture {

  std::unique_ptr<::ArgsManager> args_manager;
  std::unique_ptr<::Settings> settings;
  blockchain::Parameters parameters = blockchain::Parameters::MainNet();
  std::unique_ptr<blockchain::Behavior> behavior = blockchain::Behavior::New(args_manager.get());

  class Wallet : public staking::StakingWallet {
    mutable CCriticalSection lock;
    proposer::State state;

   public:
    CKey key;
    std::function<bool(CMutableTransaction &)> signfunc = [](CMutableTransaction &) { return false; };

    CCriticalSection &GetLock() const override { return lock; }
    CAmount GetReserveBalance() const override { return 0; }
    CAmount GetStakeableBalance() const override { return 1000; }
    std::vector<staking::Coin> GetStakeableCoins() const override { return std::vector<staking::Coin>(); }
    proposer::State &GetProposerState() override { return state; }
    boost::optional<CKey> GetKey(const CPubKey &) const override { return key; }
    bool SignCoinbaseTransaction(CMutableTransaction &tx) override { return signfunc(tx); }
  };

  Wallet wallet;

  Fixture(std::initializer_list<std::string> args)
      : args_manager([&] {
          std::unique_ptr<::ArgsManager> argsman = MakeUnique<::ArgsManager>();
          const char **argv = new const char *[args.size() + 1];
          argv[0] = "executable-name";
          std::size_t i = 1;
          for (const auto &arg : args) {
            argv[i++] = arg.c_str();
          }
          argsman->ParseParameters(static_cast<int>(i), argv);
          delete[] argv;
          return argsman;
        }()),
        settings(Settings::New(args_manager.get())) {}

  std::unique_ptr<staking::BlockValidator> MakeBlockValidator() {
    return staking::BlockValidator::New(
        behavior.get());
  }

  std::unique_ptr<proposer::BlockBuilder> MakeBlockBuilder() {
    return proposer::BlockBuilder::New(
        behavior.get(),
        settings.get());
  }
};

}  // namespace

BOOST_AUTO_TEST_SUITE(block_builder_tests)

BOOST_AUTO_TEST_CASE(build_block_and_validate) {
  Fixture f{};
  auto validator = f.MakeBlockValidator();
  auto builder = f.MakeBlockBuilder();

  uint256 block_hash = uint256();
  CBlockIndex current_tip = [&] {
    CBlockIndex ix;
    ix.phashBlock = &block_hash;
    ix.pprev = nullptr;
    ix.pskip = nullptr;
    ix.nHeight = 17;
    return ix;
  }();

  uint256 snapshot_hash;
  proposer::EligibleCoin eligible_coin;

  eligible_coin.utxo.txid = uint256();
  eligible_coin.utxo.index = 0;
  eligible_coin.utxo.amount = 100;
  eligible_coin.utxo.depth = 3;
  eligible_coin.kernel_hash = uint256();
  eligible_coin.reward = 50;
  eligible_coin.target_height = 18;
  eligible_coin.target_time = 4711;

  staking::Coin coin1;
  coin1.txid = uint256();
  coin1.index = 0;
  coin1.amount = 70;
  coin1.depth = 3;

  staking::Coin coin2;
  coin2.txid = uint256();
  coin2.index = 0;
  coin2.amount = 20;
  coin2.depth = 5;

  std::vector<staking::Coin> coins{coin1, coin2};
  std::vector<CTransactionRef> transactions;
  CAmount fees(0);

  f.wallet.signfunc = [](CMutableTransaction &) { return true; };

  auto block = builder->BuildBlock(
      current_tip, snapshot_hash, eligible_coin, coins, transactions, fees, f.wallet);
  BOOST_REQUIRE(static_cast<bool>(block));
  auto is_valid = validator->CheckBlock(*block);
  BOOST_CHECK(is_valid);
}

BOOST_AUTO_TEST_SUITE_END()
