// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_genesis.h>
#include <blockchain/blockchain_parameters.h>
#include <hash.h>
#include <staking/stake_validator.h>
#include <test/test_unite.h>
#include <uint256.h>

#include <test/test_unite.h>
#include <test/test_unite_mocks.h>
#include <boost/test/unit_test.hpp>

namespace {

struct Fixture {

  blockchain::Parameters parameters = blockchain::Parameters::MainNet();
  std::unique_ptr<blockchain::Behavior> b =
      blockchain::Behavior::NewFromParameters(parameters);

  mocks::ActiveChainMock active_chain_mock;
};

}  // namespace

BOOST_AUTO_TEST_SUITE(stake_validator_tests)

BOOST_AUTO_TEST_CASE(check_kernel) {
  Fixture fixture;
  const auto stake_validator = staking::StakeValidator::New(fixture.b.get(), &fixture.active_chain_mock);
  const uint256 kernel;
  const auto difficulty = blockchain::GenesisBlockBuilder().Build(fixture.parameters).nBits;
  BOOST_CHECK(stake_validator->CheckKernel(1, kernel, difficulty));
}

BOOST_AUTO_TEST_CASE(check_kernel_fail) {
  Fixture fixture;
  const auto stake_validator = staking::StakeValidator::New(fixture.b.get(), &fixture.active_chain_mock);
  const uint256 kernel = uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
  const auto difficulty = blockchain::GenesisBlockBuilder().Build(fixture.parameters).nBits;
  BOOST_CHECK(!stake_validator->CheckKernel(1, kernel, difficulty));
}

BOOST_AUTO_TEST_CASE(remember_and_forget) {
  Fixture fixture;
  const auto stake_validator = staking::StakeValidator::New(fixture.b.get(), &fixture.active_chain_mock);
  const uint256 txid = uint256S("000000000000000000000000e6b8347d447e02ed383a3e96986815d576fb2a5a");
  const COutPoint stake(txid, 2);
  LOCK(stake_validator->GetLock());
  BOOST_CHECK(!stake_validator->IsPieceOfStakeKnown(stake));
  stake_validator->RememberPieceOfStake(stake);
  BOOST_CHECK(stake_validator->IsPieceOfStakeKnown(stake));
  stake_validator->ForgetPieceOfStake(stake);
  BOOST_CHECK(!stake_validator->IsPieceOfStakeKnown(stake));
}

BOOST_AUTO_TEST_CASE(check_stake) {
  Fixture fixture;
  const auto stake_validator = staking::StakeValidator::New(fixture.b.get(), &fixture.active_chain_mock);

  CBlock block;

  uint256 stake_txid = uint256S("7f6b062da8f3c99f302341f06879ff94db0b7ae291b38438846c9878b58412d4");
  std::uint32_t stake_ix = 7;

  CScript stake_script_sig;
  COutPoint stake_ref(stake_txid, stake_ix);
  CTxIn stake(stake_ref, stake_script_sig);

  CMutableTransaction tx;
  tx.vin = {stake};

  LOCK(fixture.active_chain_mock.GetLock());
  stake_validator->CheckStake(block, nullptr);
}

BOOST_AUTO_TEST_CASE(check_remote_staking_outputs) {
  Fixture fixture;
  const auto stake_validator = staking::StakeValidator::New(fixture.b.get(), &fixture.active_chain_mock);

  CBlock block;
  block.nTime = 1550507858;

  CBlockIndex prev_block;
  prev_block.nTime = block.nTime - 15;
  prev_block.stake_modifier = uint256S("2cdcf27ffe49aa00d95605c677a38462b684763b7218c6dbd856293bf8325cd0");

  fixture.active_chain_mock.get_block_index = [&prev_block](const uint256 &) { return &prev_block; };

  uint256 stake_txid = uint256S("7f6b062da8f3c99f302341f06879ff94db0b7ae291b38438846c9878b58412d4");
  COutPoint stake_ref(stake_txid, 7);
  CTxIn stake(stake_ref, CScript());

  COutPoint input2_ref(stake_txid, 2);
  CTxIn input2(input2_ref, CScript());

  const CScript script = CScript() << OP_1 << std::vector<uint8_t>(20) << std::vector<uint8_t>(32);
  const CScript script2 = CScript() << OP_2 << std::vector<uint8_t>(20, 1) << std::vector<uint8_t>(32);
  blockchain::Depth depth = fixture.parameters.stake_maturity + 10;

  std::map<COutPoint, staking::Coin> coins;
  coins[stake_ref] = {stake_ref.hash, stake_ref.n, UNIT, script, depth};

  fixture.active_chain_mock.get_utxo = [&coins](const COutPoint &p) {
    const auto it = coins.find(p);
    return it == coins.end() ? boost::none : boost::make_optional(it->second);
  };

  fixture.parameters.difficulty_function = [](const blockchain::Parameters &p, blockchain::Height h,
                                              blockchain::ChainAccess &c) -> blockchain::Difficulty {
    return 0x1d000fff;
  };

  CMutableTransaction tx;
  tx.vin = {CTxIn(), stake};
  tx.SetType(TxType::COINBASE);

  LOCK(fixture.active_chain_mock.GetLock());
  {
    const CTxOut out(UNIT, script);
    tx.vout = {out};
    block.vtx = {MakeTransactionRef(tx)};

    const auto r = stake_validator->CheckStake(block, nullptr);
    BOOST_CHECK_MESSAGE(r, r.GetRejectionMessage());
  }

  {
    const CTxOut out(UNIT - 1, script);

    tx.vout = {out};
    block.vtx = {MakeTransactionRef(tx)};

    const auto r = stake_validator->CheckStake(block, nullptr);
    BOOST_CHECK(!r);
    BOOST_CHECK(r.errors.Contains(staking::BlockValidationError::REMOTE_STAKING_INPUT_BIGGER_THAN_OUTPUT));
  }

  {
    const CTxOut out(UNIT - 10000, script);
    const CTxOut out2(10100, script);

    tx.vout = {out, out2};
    block.vtx = {MakeTransactionRef(tx)};

    const auto r = stake_validator->CheckStake(block, nullptr);
    BOOST_CHECK_MESSAGE(r, r.GetRejectionMessage());
  }

  tx.vin.push_back(input2);
  {
    const CTxOut out(3 * UNIT, script);

    tx.vout = {out};
    block.vtx = {MakeTransactionRef(tx)};

    const auto r = stake_validator->CheckStake(block, nullptr);
    BOOST_CHECK(!r);
    BOOST_CHECK(r.errors.Contains(staking::BlockValidationError::TRANSACTION_INPUT_NOT_FOUND));
  }

  coins[input2_ref] = {input2_ref.hash, input2_ref.n, 2 * UNIT, script2, depth};

  {
    const CTxOut out(UNIT, script);
    const CTxOut out2(2 * UNIT, script2);

    tx.vout = {out, out2};
    block.vtx = {MakeTransactionRef(tx)};

    const auto r = stake_validator->CheckStake(block, nullptr);
    BOOST_CHECK_MESSAGE(r, r.GetRejectionMessage());
  }

  {
    const CTxOut out(2 * UNIT, script);
    const CTxOut out2(UNIT, script2);

    tx.vout = {out, out2};
    block.vtx = {MakeTransactionRef(tx)};

    const auto r = stake_validator->CheckStake(block, nullptr);
    BOOST_CHECK(!r);
    BOOST_CHECK(r.errors.Contains(staking::BlockValidationError::REMOTE_STAKING_INPUT_BIGGER_THAN_OUTPUT));
  }
}

BOOST_AUTO_TEST_SUITE_END()
