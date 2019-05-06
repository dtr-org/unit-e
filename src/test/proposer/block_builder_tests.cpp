// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/block_builder.h>

#include <blockchain/blockchain_behavior.h>
#include <key_io.h>
#include <primitives/transaction.h>
#include <staking/block_validator.h>
#include <wallet/wallet.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

#include <test/test_unite_mocks.h>
#include <algorithm>
#include <cstdlib>

namespace {

struct Fixture {

  std::unique_ptr<::ArgsManager> args_manager;
  std::unique_ptr<blockchain::Behavior> behavior = blockchain::Behavior::New(args_manager.get());
  std::unique_ptr<::Settings> settings;
  blockchain::Parameters parameters = blockchain::Parameters::TestNet();

  uint256 snapshot_hash = uint256::zero;

  const CBlockIndex block = [] {
    CBlockIndex index;
    index.nHeight = 3;
    return index;
  }();

  const uint256 txid;

  const proposer::EligibleCoin eligible_coin{
      staking::Coin(&block, {txid, 0}, {100, GetScriptForDestination(CKeyID())}),
      uint256(),
      CAmount(50),
      blockchain::Height(18),
      behavior->CalculateProposingTimestampAfter(4711),
      blockchain::Difficulty(0x20FF00)};

  class Wallet : public staking::StakingWallet {
    mutable CCriticalSection lock;
    proposer::State state;
    std::string name = "test-wallet";

   public:
    CKey key;
    std::function<bool(CMutableTransaction &)> signfunc = [](CMutableTransaction &) { return false; };

    CCriticalSection &GetLock() const override { return lock; }
    CAmount GetReserveBalance() const override { return 0; }
    CAmount GetStakeableBalance() const override { return 1000; }
    staking::CoinSet GetStakeableCoins() const override { return staking::CoinSet(); }
    proposer::State &GetProposerState() override { return state; }
    boost::optional<CKey> GetKey(const CPubKey &) const override { return key; }
    bool SignCoinbaseTransaction(CMutableTransaction &tx) override { return signfunc(tx); }
    const std::string &GetName() const override { return name; }
  };

  Wallet wallet;

  CKey key;
  CPubKey pubkey;
  std::vector<unsigned char> pubkeydata;

  Fixture(std::initializer_list<std::string> args)
      : args_manager(new mocks::ArgsManagerMock(args)),
        settings(Settings::New(args_manager.get(), behavior.get())) {

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

  std::unique_ptr<staking::BlockValidator> MakeBlockValidator() {
    return staking::BlockValidator::New(
        behavior.get());
  }

  std::unique_ptr<proposer::BlockBuilder> MakeBlockBuilder() {
    return proposer::BlockBuilder::New(settings.get());
  }
};

}  // namespace

BOOST_FIXTURE_TEST_SUITE(block_builder_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(build_block_and_validate) {
  Fixture f{};
  auto validator = f.MakeBlockValidator();
  auto builder = f.MakeBlockBuilder();

  const uint256 block_hash = uint256::zero;
  const CBlockIndex current_tip = [&] {
    CBlockIndex ix;
    ix.phashBlock = &block_hash;
    ix.pprev = nullptr;
    ix.pskip = nullptr;
    ix.nHeight = 17;
    return ix;
  }();

  const CBlockIndex block1 = [&] {
    CBlockIndex index;
    index.nHeight = current_tip.nHeight - 3;
    return index;
  }();

  const CBlockIndex block2 = [&] {
    CBlockIndex index;
    index.nHeight = current_tip.nHeight - 5;
    return index;
  }();

  const staking::Coin coin1(&block1, {uint256::zero, 0}, {70, CScript()});
  const staking::Coin coin2(&block2, {uint256::zero, 0}, {20, CScript()});

  staking::CoinSet coins;
  coins.insert(coin1);
  coins.insert(coin2);
  std::vector<CTransactionRef> transactions;
  CAmount fees(0);

  auto block = builder->BuildBlock(
      current_tip, f.snapshot_hash, f.eligible_coin, coins, transactions, fees, f.wallet);
  BOOST_REQUIRE(static_cast<bool>(block));
  auto is_valid = validator->CheckBlock(*block, nullptr);
  BOOST_CHECK(is_valid);

  auto &stake_in = block->vtx[0]->vin[1];
  BOOST_CHECK(stake_in.scriptWitness.stack[1] == f.pubkeydata);

  std::vector<uint8_t> signature;
  f.key.Sign(block->GetHash(), signature);

  BOOST_CHECK(signature == block->signature);
}

BOOST_AUTO_TEST_CASE(split_amount) {
  auto split_amount_test = [&](int split_threshold, int expected_outputs) -> void {
    Fixture f{tfm::format("-stakesplitthreshold=%d", split_threshold)};
    std::unique_ptr<staking::BlockValidator> validator = f.MakeBlockValidator();
    std::unique_ptr<proposer::BlockBuilder> builder = f.MakeBlockBuilder();

    const uint256 block_hash = uint256::zero;
    const CBlockIndex current_tip = [&] {
      CBlockIndex ix;
      ix.phashBlock = &block_hash;
      ix.pprev = nullptr;
      ix.pskip = nullptr;
      ix.nHeight = 17;
      return ix;
    }();

    staking::CoinSet coins;  // no other coins
    std::vector<CTransactionRef> transactions;
    CAmount fees(0);

    std::shared_ptr<const CBlock> block = builder->BuildBlock(
        current_tip, f.snapshot_hash, f.eligible_coin, coins, transactions, fees, f.wallet);
    BOOST_REQUIRE(static_cast<bool>(block));
    const staking::BlockValidationResult is_valid = validator->CheckBlock(*block, nullptr);
    // must have a coinbase transaction
    BOOST_REQUIRE(!block->vtx.empty());
    const CTransactionRef coinbase = block->vtx[0];
    const std::vector<CTxOut> &outputs = coinbase->vout;
    BOOST_CHECK_EQUAL(outputs.size(), expected_outputs);

    // start at outputs.begin() + 1 to skip reward output which is not subject to stake splitting (it's not stake)
    auto minmax = std::minmax_element(outputs.begin() + 1, outputs.end(), [](const CTxOut &left, const CTxOut &right) {
      return left.nValue < right.nValue;
    });
    // check that outputs differ no more than one in size (this avoids dust)
    BOOST_CHECK(abs(minmax.first->nValue - minmax.second->nValue) <= 1);
    BOOST_CHECK(static_cast<bool>(is_valid));
  };

  // eligible_coin.amount=100, threshold=10, reward=50 -> 10x10 (reward is separate)
  split_amount_test(10, 11);  // 10 + 1

  // no piece bigger than 70
  split_amount_test(70, 3);

  // check that dust is avoided
  split_amount_test(149, 2);
}

BOOST_AUTO_TEST_CASE(check_reward_destination) {

  const std::string address("tue1qdkcn53gqgnmtfxy8hl24kdev5axv20evp23a6x");
  const CTxDestination expected_reward_dest = DecodeDestination(address);
  Fixture f{tfm::format("-rewardaddress=%s", address)};
  std::unique_ptr<staking::BlockValidator> validator = f.MakeBlockValidator();
  std::unique_ptr<proposer::BlockBuilder> builder = f.MakeBlockBuilder();

  const uint256 block_hash = uint256();
  CBlockIndex current_tip = [&] {
    CBlockIndex ix;
    ix.phashBlock = &block_hash;
    ix.pprev = nullptr;
    ix.pskip = nullptr;
    ix.nHeight = 17;
    return ix;
  }();

  staking::CoinSet coins;
  coins.insert(f.eligible_coin.utxo);
  std::vector<CTransactionRef> transactions;
  CAmount fees(5);

  std::shared_ptr<const CBlock> block = builder->BuildBlock(
      current_tip, f.snapshot_hash, f.eligible_coin, coins, transactions, fees, f.wallet);
  BOOST_REQUIRE(static_cast<bool>(block));
  const staking::BlockValidationResult is_valid = validator->CheckBlock(*block, nullptr);
  BOOST_CHECK(static_cast<bool>(is_valid));

  const CTxOut stake_out = block->vtx[0]->vout[1];
  BOOST_CHECK_EQUAL(f.eligible_coin.utxo.GetAmount(), stake_out.nValue);

  const CAmount to_address_reward = block->vtx[0]->vout[0].nValue;
  BOOST_CHECK_EQUAL(to_address_reward, f.eligible_coin.reward + fees);

  CTxDestination reward_dest;
  ExtractDestination(block->vtx[0]->vout[0].scriptPubKey, reward_dest);
  BOOST_CHECK(expected_reward_dest == reward_dest);
}

BOOST_AUTO_TEST_CASE(combine_stake) {
  Fixture f{tfm::format("-stakesplitthreshold=%d", 0),
            tfm::format("-stakecombinemaximum=%d", 210)};
  std::unique_ptr<staking::BlockValidator> validator = f.MakeBlockValidator();
  std::unique_ptr<proposer::BlockBuilder> builder = f.MakeBlockBuilder();

  const uint256 block_hash = uint256::zero;
  const CBlockIndex current_tip = [&] {
    CBlockIndex ix;
    ix.phashBlock = &block_hash;
    ix.pprev = nullptr;
    ix.pskip = nullptr;
    ix.nHeight = 17;
    return ix;
  }();

  const staking::Coin coin1(&f.block, {uint256::zero, 1}, {120, CScript()});
  const staking::Coin coin2(&f.block, {uint256::zero, 2}, {60, CScript()});
  const staking::Coin coin3(&f.block, {uint256::zero, 3}, {40, CScript()});
  staking::CoinSet coins{f.eligible_coin.utxo, coin1, coin2, coin3};
  std::vector<CTransactionRef> transactions;
  CAmount fees(0);

  std::shared_ptr<const CBlock> block = builder->BuildBlock(
      current_tip, f.snapshot_hash, f.eligible_coin, coins, transactions, fees, f.wallet);
  BOOST_REQUIRE(static_cast<bool>(block));
  const staking::BlockValidationResult is_valid = validator->CheckBlock(*block, nullptr);
  BOOST_CHECK(static_cast<bool>(is_valid));
  // must have a coinbase transaction
  BOOST_REQUIRE(!block->vtx.empty());

  const std::vector<CTxIn> &inputs = block->vtx[0]->vin;
  BOOST_REQUIRE_EQUAL(inputs.size(), 2 + 2);  // meta input + stake + 2 coins to combine with stake
  BOOST_CHECK(inputs[1].prevout == f.eligible_coin.utxo.GetOutPoint());
  BOOST_CHECK(
      (inputs[2].prevout == coin2.GetOutPoint() && inputs[3].prevout == coin3.GetOutPoint()) ||
      (inputs[3].prevout == coin2.GetOutPoint() && inputs[2].prevout == coin3.GetOutPoint()));

  const std::vector<CTxOut> &outputs = block->vtx[0]->vout;
  BOOST_CHECK_EQUAL(outputs.size(), 2);
  BOOST_CHECK_EQUAL(outputs[1].nValue, f.eligible_coin.utxo.GetAmount() + coin2.GetAmount() + coin3.GetAmount());
}

// TODO UNIT-E: check that combining skipped when staking remote staking outputs

BOOST_AUTO_TEST_CASE(remote_staking) {
  Fixture f{tfm::format("-stakesplitthreshold=%d", 0),
            tfm::format("-stakecombinemaximum=%d", 210)};
  std::unique_ptr<staking::BlockValidator> validator = f.MakeBlockValidator();
  std::unique_ptr<proposer::BlockBuilder> builder = f.MakeBlockBuilder();

  const uint256 block_hash = uint256::zero;
  const CBlockIndex current_tip = [&] {
    CBlockIndex ix;
    ix.phashBlock = &block_hash;
    ix.pprev = nullptr;
    ix.pskip = nullptr;
    ix.nHeight = 17;
    return ix;
  }();

  const auto rsp2wpkh = CScript::CreateRemoteStakingKeyhashScript(std::vector<unsigned char>(20), std::vector<unsigned char>(32));

  const staking::Coin coin1(&f.block, {uint256::zero, 1}, {20, CScript()});
  const staking::Coin coin2(&f.block, {uint256::zero, 2}, {20, CScript()});
  std::vector<CTransactionRef> transactions;
  CAmount fees(0);

  // Do not combine coins when stake is a remote-staking output
  {
    const proposer::EligibleCoin eligible_coin{
        staking::Coin(&f.block, {f.txid, 10}, {100, rsp2wpkh}),
        uint256::zero,
        CAmount(50),
        blockchain::Height(18),
        f.behavior->CalculateProposingTimestampAfter(4711),
        blockchain::Difficulty(0x20FF00)};

    staking::CoinSet coins{eligible_coin.utxo, coin1, coin2};

    std::shared_ptr<const CBlock> block = builder->BuildBlock(
        current_tip, f.snapshot_hash, eligible_coin, coins, transactions, fees, f.wallet);
    BOOST_REQUIRE(static_cast<bool>(block));
    const staking::BlockValidationResult is_valid = validator->CheckBlock(*block, nullptr);
    BOOST_CHECK(static_cast<bool>(is_valid));
    // must have a coinbase transaction
    BOOST_REQUIRE(!block->vtx.empty());

    const std::vector<CTxIn> &inputs = block->vtx[0]->vin;
    BOOST_CHECK_EQUAL(inputs.size(), 2);  // meta input + stake
    BOOST_CHECK(inputs[1].prevout == eligible_coin.utxo.GetOutPoint());

    const std::vector<CTxOut> &outputs = block->vtx[0]->vout;
    BOOST_CHECK_EQUAL(outputs.size(), 2);
    BOOST_CHECK_EQUAL(outputs[1].nValue, eligible_coin.utxo.GetAmount());
  }

  // Do not combine remote-staking coins
  {
    const auto rsp2wsh = CScript::CreateRemoteStakingScripthashScript(std::vector<unsigned char>(20), std::vector<unsigned char>(32));
    const staking::Coin rs_coin1(&f.block, {uint256::zero, 4}, {25, rsp2wpkh});
    const staking::Coin rs_coin2(&f.block, {uint256::zero, 5}, {25, rsp2wsh});
    staking::CoinSet coins{f.eligible_coin.utxo, coin1, coin2, rs_coin1, rs_coin2};

    std::shared_ptr<const CBlock> block = builder->BuildBlock(
        current_tip, f.snapshot_hash, f.eligible_coin, coins, transactions, fees, f.wallet);
    BOOST_REQUIRE(static_cast<bool>(block));
    const staking::BlockValidationResult is_valid = validator->CheckBlock(*block, nullptr);
    BOOST_CHECK(static_cast<bool>(is_valid));
    // must have a coinbase transaction
    BOOST_REQUIRE(!block->vtx.empty());

    const std::vector<CTxIn> &inputs = block->vtx[0]->vin;
    BOOST_REQUIRE_EQUAL(inputs.size(), 2 + 2);  // meta input + stake + 2 coins to combine with stake
    BOOST_CHECK(inputs[1].prevout == f.eligible_coin.utxo.GetOutPoint());
    BOOST_CHECK(
        (inputs[2].prevout == coin1.GetOutPoint() && inputs[3].prevout == coin2.GetOutPoint()) ||
        (inputs[3].prevout == coin1.GetOutPoint() && inputs[2].prevout == coin2.GetOutPoint()));

    const std::vector<CTxOut> &outputs = block->vtx[0]->vout;
    BOOST_CHECK_EQUAL(outputs.size(), 2);
    BOOST_CHECK_EQUAL(outputs[1].nValue, f.eligible_coin.utxo.GetAmount() + coin1.GetAmount() + coin2.GetAmount());
  }
}

BOOST_AUTO_TEST_SUITE_END()
