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

#include <algorithm>
#include <cstdlib>

namespace {

struct Fixture {

  std::unique_ptr<::ArgsManager> args_manager;
  std::unique_ptr<::Settings> settings;
  blockchain::Parameters parameters = blockchain::Parameters::MainNet();
  std::unique_ptr<blockchain::Behavior> behavior = blockchain::Behavior::New(args_manager.get());

  uint256 snapshot_hash = uint256();

  proposer::EligibleCoin eligible_coin = [&] {
    proposer::EligibleCoin coin;
    coin.utxo.txid = uint256();
    coin.utxo.index = 0;
    coin.utxo.amount = 100;
    coin.utxo.depth = 3;
    coin.kernel_hash = uint256();
    coin.reward = 50;
    coin.target_height = 18;
    coin.target_time = behavior->CalculateProposingTimestampAfter(4711);
    return coin;
  }();

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

  CKey key;
  CPubKey pubkey;
  std::vector<unsigned char> pubkeydata;

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
        settings(Settings::New(args_manager.get())) {

    const key::mnemonic::Seed seed("scout wheat rhythm inmate make insect chimney interest fire oxygen gap party slush grid post");
    const CExtKey &ext_key = seed.GetExtKey();
    // public key for signing block
    key = ext_key.key;
    pubkey = key.GetPubKey();
    pubkeydata = std::vector<unsigned char>(pubkey.begin(), pubkey.end());
  }

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

BOOST_FIXTURE_TEST_SUITE(block_builder_tests, BasicTestingSetup)

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

  f.wallet.key = f.key;
  f.wallet.signfunc = [&](CMutableTransaction &tx) {
    auto &witness_stack = tx.vin[1].scriptWitness.stack;
    witness_stack.emplace_back();              // empty signature
    witness_stack.emplace_back(f.pubkeydata);  // pubkey
    return true;
  };

  auto block = builder->BuildBlock(
      current_tip, f.snapshot_hash, f.eligible_coin, coins, transactions, fees, f.wallet);
  BOOST_REQUIRE(static_cast<bool>(block));
  auto is_valid = validator->CheckBlock(*block);
  BOOST_CHECK(is_valid);
}

BOOST_AUTO_TEST_CASE(split_amount) {
  auto split_amount_test = [&](int split_threshold, int expected_outputs) -> void {
    Fixture f{tfm::format("-stakesplitthreshold=%d", split_threshold)};
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

    std::vector<staking::Coin> coins;  // no other coins
    std::vector<CTransactionRef> transactions;
    CAmount fees(0);

    f.wallet.key = f.key;
    f.wallet.signfunc = [&](CMutableTransaction &tx) {
      auto &witness_stack = tx.vin[1].scriptWitness.stack;
      witness_stack.emplace_back();              // empty signature
      witness_stack.emplace_back(f.pubkeydata);  // pubkey
      return true;
    };

    auto block = builder->BuildBlock(
        current_tip, f.snapshot_hash, f.eligible_coin, coins, transactions, fees, f.wallet);
    BOOST_REQUIRE(static_cast<bool>(block));
    auto is_valid = validator->CheckBlock(*block);
    // must have a coinbase transaction
    BOOST_REQUIRE(!block->vtx.empty());
    auto coinbase = block->vtx[0];
    auto &outputs = coinbase->vout;
    BOOST_CHECK_EQUAL(outputs.size(), expected_outputs);

    auto minmax = std::minmax_element(outputs.begin(), outputs.end(), [](const CTxOut &left, const CTxOut &right) {
      return left.nValue < right.nValue;
    });
    // check that outputs differ no more than one in size (this avoids dust)
    BOOST_CHECK(abs(minmax.first->nValue - minmax.second->nValue) <= 1);
    BOOST_CHECK(is_valid);
  };

  // eligible_coin.amount=100, reward=50, outsum=150 -> 10x15
  split_amount_test(10, 15);

  // no piece bigger than 70
  split_amount_test(70, 3);

  // check that dust is avoided
  split_amount_test(149, 2);
}

BOOST_AUTO_TEST_SUITE_END()
