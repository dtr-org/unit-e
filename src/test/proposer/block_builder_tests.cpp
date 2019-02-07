// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/block_builder.h>

#include <blockchain/blockchain_behavior.h>
#include <blockdb.h>
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
  std::unique_ptr<::BlockDB> block_db;
  blockchain::Parameters parameters = blockchain::Parameters::MainNet();
  std::unique_ptr<blockchain::Behavior> behavior = blockchain::Behavior::New(args_manager.get());

  uint256 snapshot_hash = GetRandHash();

  proposer::EligibleCoin eligible_coin = [&] {
    proposer::EligibleCoin coin;
    coin.utxo.txid = GetRandHash();
    coin.utxo.index = 0;
    coin.utxo.amount = 100 * UNIT;
    coin.utxo.script_pubkey = GetScriptForDestination(CKeyID());
    coin.utxo.depth = 3;
    coin.kernel_hash = GetRandHash();
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

  template <typename... Args>
  explicit Fixture(CBlock &block, Args... args) : Fixture(block, {args...}) {}
  explicit Fixture(CBlock &block) : Fixture(block, {}) {}

  static CBlock CreateBlock(CScript script_sig = CScript()) {
    CMutableTransaction coinbase;
    coinbase.SetType(TxType::COINBASE);

    if (script_sig.empty()) {
      script_sig = CScript() << CScriptNum::serialize(143012)
                             << ToByteVector(GetRandHash())
                             << 0;
    }
    coinbase.vin.emplace_back(GetRandHash(), 0, script_sig);

    CBlock block;
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));

    return block;
  }

  std::unique_ptr<staking::BlockValidator> MakeBlockValidator() {
    return staking::BlockValidator::New(
        behavior.get());
  }

  std::unique_ptr<proposer::BlockBuilder> MakeBlockBuilder() {
    return proposer::BlockBuilder::New(
        behavior.get(),
        block_db.get(),
        settings.get());
  }

  static std::unique_ptr<BlockDB> MakeBlockDB(CBlock &block) {

    class MockDB : public BlockDB {

     private:
      CBlock block;

     public:
      explicit MockDB(CBlock block) {
        this->block = block;
      }

      boost::optional<CBlock> ReadBlock(const CBlockIndex &index) override {
        return block;
      };

      ~MockDB() override = default;

      static std::unique_ptr<BlockDB> New(CBlock &block) {
        return std::unique_ptr<BlockDB>(new MockDB(block));
      };
    };
    return MockDB::New(block);
  }

 private:
  Fixture(CBlock &block, std::initializer_list<std::string> args)
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
        block_db(MakeBlockDB(block)),
        settings(Settings::New(args_manager.get())) {

    const key::mnemonic::Seed seed("scout wheat rhythm inmate make insect chimney interest fire oxygen gap party slush grid post");
    const CExtKey &ext_key = seed.GetExtKey();
    // public key for signing block
    key = ext_key.key;
    pubkey = key.GetPubKey();
    pubkeydata = std::vector<unsigned char>(pubkey.begin(), pubkey.end());

    wallet.key = key;
    wallet.signfunc = [&](CMutableTransaction &tx) {
      auto &witness_stack = tx.vin[1].scriptWitness.stack;
      witness_stack.emplace_back();            // empty signature
      witness_stack.emplace_back(pubkeydata);  // pubkey
      return true;
    };
  }
};

}  // namespace

BOOST_FIXTURE_TEST_SUITE(block_builder_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(build_block_and_validate) {

  auto prev_block = Fixture::CreateBlock();
  Fixture f(prev_block);

  auto validator = f.MakeBlockValidator();
  auto builder = f.MakeBlockBuilder();

  uint256 block_hash = GetRandHash();
  CBlockIndex current_tip = [&] {
    CBlockIndex ix;
    ix.phashBlock = &block_hash;
    ix.pprev = nullptr;
    ix.pskip = nullptr;
    ix.nHeight = 17;
    return ix;
  }();

  uint256 tx_id = GetRandHash();
  staking::Coin coin1;
  coin1.txid = tx_id;
  coin1.index = 0;
  coin1.amount = 70;
  coin1.depth = 3;

  staking::Coin coin2;
  coin2.txid = tx_id;
  coin2.index = 1;
  coin2.amount = 20;
  coin2.depth = 5;

  staking::Coin coin3;
  coin2.txid = GetRandHash();
  coin2.index = 0;
  coin2.amount = 20;
  coin2.depth = 5;

  std::vector<staking::Coin> coins{coin1, coin2, coin3};
  std::vector<CTransactionRef> transactions;
  CAmount fees(0);

  auto block = builder->BuildBlock(
      current_tip, f.snapshot_hash, f.eligible_coin, coins, transactions, fees, f.wallet);
  BOOST_REQUIRE(static_cast<bool>(block));
  auto is_valid = validator->CheckBlock(*block);
  BOOST_CHECK(is_valid);

  BOOST_CHECK(block->vtx[0]->GetType() == +TxType::COINBASE);

  BOOST_CHECK_EQUAL(1, block->vtx.size());
  BOOST_CHECK_EQUAL(5, block->vtx[0]->vin.size());  // coins + eligible_coin + meta
  BOOST_CHECK_EQUAL(1, block->vtx[0]->vout.size());

  auto &stake_in = block->vtx[0]->vin[1];
  BOOST_CHECK(stake_in.scriptWitness.stack[1] == f.pubkeydata);

  std::vector<uint8_t> signature;
  f.key.Sign(block->GetHash(), signature);

  BOOST_CHECK(signature == block->signature);
}

BOOST_AUTO_TEST_CASE(extract_validators_fund) {

  CAmount fund = 751 * UNIT;
  CScript script_sig = CScript() << CScriptNum::serialize(143012)
                                 << ToByteVector(GetRandHash())
                                 << fund;

  auto block = Fixture::CreateBlock(script_sig);
  CAmount extracted_fund;
  BOOST_CHECK(proposer::BlockBuilder::GetValidatorsFund(block, extracted_fund));
  BOOST_CHECK_EQUAL(fund, extracted_fund);
}

BOOST_AUTO_TEST_CASE(validators_fund_and_block_reward) {

  CAmount initial_validators_fund = 10000 * UNIT;

  CScript script_sig = CScript() << CScriptNum::serialize(143012)
                                 << ToByteVector(GetRandHash())
                                 << initial_validators_fund;

  auto prev_block = Fixture::CreateBlock(script_sig);

  Fixture f(prev_block);
  auto builder = f.MakeBlockBuilder();

  CAmount fees = 1412331;

  auto block_reward = f.behavior->CalculateBlockReward(1, fees);
  CAmount validators_fund_reward = block_reward.validator_funds;

  uint256 prev_block_hash = prev_block.GetHash();
  CBlockIndex current_tip = [&, prev_block] {
    CBlockIndex ix;
    ix.phashBlock = &prev_block_hash;
    ix.pprev = nullptr;
    ix.pskip = nullptr;
    ix.nHeight = 17;
    return ix;
  }();

  staking::Coin coin;
  coin.txid = GetRandHash();
  coin.index = 0;
  coin.amount = 70;
  coin.depth = 3;

  // Checking that if passed, the eligible coin does not get counted twice
  std::vector<staking::Coin> coins{f.eligible_coin.utxo, coin};
  std::vector<CTransactionRef> transactions;

  auto block = builder->BuildBlock(
      current_tip, f.snapshot_hash, f.eligible_coin, coins, transactions, fees, f.wallet);

  //Check that the validators fund is present
  CAmount final_validators_fund;
  BOOST_CHECK(proposer::BlockBuilder::GetValidatorsFund(*block, final_validators_fund));
  BOOST_CHECK_EQUAL(initial_validators_fund + validators_fund_reward, final_validators_fund);

  //Check that the correct block reward is in the coinbase
  const CAmount coinbase_block_rewards = block->vtx[0]->vout[0].nValue;
  BOOST_CHECK_EQUAL(block_reward.immediate_reward + coin.amount + f.eligible_coin.utxo.amount, coinbase_block_rewards);
}

BOOST_AUTO_TEST_CASE(split_amount) {
  auto prev_block = Fixture::CreateBlock();

  auto split_amount_test = [&](CAmount split_threshold, int expected_outputs) -> void {
    Fixture f{prev_block, tfm::format("-stakesplitthreshold=%d", split_threshold)};

    auto validator = f.MakeBlockValidator();
    auto builder = f.MakeBlockBuilder();

    uint256 block_hash = GetRandHash();
    CBlockIndex current_tip = [&] {
      CBlockIndex ix;
      ix.phashBlock = &block_hash;
      ix.pprev = nullptr;
      ix.pskip = nullptr;
      ix.nHeight = 17;
      return ix;
    }();

    std::vector<staking::Coin> coins;  // no other coins
    std::vector<CTransactionRef> transactions;
    CAmount fees(0);

    std::shared_ptr<const CBlock> block = builder->BuildBlock(
        current_tip, f.snapshot_hash, f.eligible_coin, coins, transactions, fees, f.wallet);
    BOOST_REQUIRE(static_cast<bool>(block));
    const staking::BlockValidationResult is_valid = validator->CheckBlock(*block);
    // must have a coinbase transaction
    BOOST_REQUIRE(!block->vtx.empty());
    const CTransactionRef coinbase = block->vtx[0];
    const std::vector<CTxOut> &outputs = coinbase->vout;
    BOOST_CHECK_EQUAL(outputs.size(), expected_outputs);

    auto minmax = std::minmax_element(outputs.begin(), outputs.end(), [](const CTxOut &left, const CTxOut &right) {
      return left.nValue < right.nValue;
    });
    // check that outputs differ no more than one in size (this avoids dust)
    BOOST_CHECK(abs(minmax.first->nValue - minmax.second->nValue) <= 1);
    BOOST_CHECK(static_cast<bool>(is_valid));
  };

  // eligible_coin.amount=100, reward=3.75, outsum=103.75
  split_amount_test(1037500000, 10);

  // no piece bigger than 50
  split_amount_test(50 * UNIT, 3);

  // check that dust is avoided
  split_amount_test(103 * UNIT, 2);
}

BOOST_AUTO_TEST_CASE(check_reward_destination) {

  const std::string address("bc1q2znaq2pnwqg92lpsc9sf7p5z0drluu27jn2a92");
  const CTxDestination expected_reward_dest = DecodeDestination(address);

  auto prev_block = Fixture::CreateBlock();
  Fixture f{prev_block, tfm::format("-rewardaddress=%s", address)};

  auto validator = f.MakeBlockValidator();
  auto builder = f.MakeBlockBuilder();

  const uint256 block_hash = GetRandHash();
  CBlockIndex current_tip = [&] {
    CBlockIndex ix;
    ix.phashBlock = &block_hash;
    ix.pprev = nullptr;
    ix.pskip = nullptr;
    ix.nHeight = 17;
    return ix;
  }();

  staking::Coin coin;
  coin.txid = GetRandHash();
  coin.index = 0;
  coin.amount = 70;
  coin.depth = 3;

  std::vector<staking::Coin> coins{coin};
  std::vector<CTransactionRef> transactions;
  CAmount fees(5);

  std::shared_ptr<const CBlock> block = builder->BuildBlock(
      current_tip, f.snapshot_hash, f.eligible_coin, coins, transactions, fees, f.wallet);
  BOOST_REQUIRE(static_cast<bool>(block));
  const staking::BlockValidationResult is_valid = validator->CheckBlock(*block);
  BOOST_CHECK(static_cast<bool>(is_valid));

  BOOST_CHECK_EQUAL(1, block->vtx.size());
  BOOST_CHECK_EQUAL(3, block->vtx[0]->vin.size());  // coins + eligible_coin + meta
  BOOST_CHECK_EQUAL(2, block->vtx[0]->vout.size());

  const CTxOut stake_out = block->vtx[0]->vout[0];
  BOOST_CHECK_EQUAL(f.eligible_coin.utxo.amount + coin.amount, stake_out.nValue);

  auto block_reward = f.behavior->CalculateBlockReward(1, fees);
  const CAmount to_address_reward = block->vtx[0]->vout[1].nValue;
  BOOST_CHECK_EQUAL(block_reward.immediate_reward, to_address_reward);

  CTxDestination reward_dest;
  ExtractDestination(block->vtx[0]->vout[1].scriptPubKey, reward_dest);
  BOOST_CHECK(expected_reward_dest == reward_dest);
}

BOOST_AUTO_TEST_SUITE_END()
