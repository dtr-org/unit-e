// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/validation.h>
#include <staking/coin.h>
#include <test/test_unite.h>
#include <validationinterface.h>
#include <wallet/test/wallet_test_fixture.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(new_pos_valid_block_tests, TestChain100Setup)

struct NewPoSValidBlockListener : CValidationInterface {
  NewPoSValidBlockListener() {
    RegisterValidationInterface(this);
  }

  ~NewPoSValidBlockListener() {
    UnregisterValidationInterface(this);
  }

  std::set<uint256> new_pos_valid_blocks;
  std::map<uint256, CValidationState> checked_blocks;

 protected:
  void NewPoSValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock> &block) override {
    new_pos_valid_blocks.emplace(block->GetHash());
  }

  void BlockChecked(const CBlock &block, const CValidationState &state) override {
    checked_blocks[block.GetHash()] = state;
  }
};

staking::Coin GetStake(CWallet &wallet) {
  LOCK2(cs_main, wallet.cs_wallet);
  const esperanza::WalletExtension &wallet_ext = wallet.GetWalletExtension();
  return *wallet_ext.GetStakeableCoins().begin();
}

BOOST_AUTO_TEST_CASE(spent_stake) {
  const staking::Coin stake = GetStake(*m_wallet);

  const CScript coinbase_script = CScript() << OP_1;

  bool processed;
  NewPoSValidBlockListener listener;
  const CBlock valid_block = CreateAndProcessBlock({}, coinbase_script, stake, &processed);
  BOOST_CHECK(processed);
  BOOST_CHECK_EQUAL(1, listener.new_pos_valid_blocks.count(valid_block.GetHash()));

  // Using the same stake again => spent stake
  const CBlock invalid_block = CreateAndProcessBlock({}, coinbase_script, stake, &processed);
  BOOST_CHECK(!processed);
  BOOST_CHECK_EQUAL(0, listener.new_pos_valid_blocks.count(invalid_block.GetHash()));
}

BOOST_AUTO_TEST_CASE(invalid_stake_script) {
  const CScript coinbase_script = CScript() << OP_1;
  std::shared_ptr<const CBlock> valid_block = CreateBlock({}, coinbase_script);

  // We are going to alter some coinbase scripts to make this block invalid
  auto invalid_block = std::make_shared<CBlock>(*valid_block);
  auto coinbase = CMutableTransaction(*invalid_block->vtx[0]);
  coinbase.vin[1].scriptSig = CScript() << OP_1;
  invalid_block->vtx[0] = MakeTransactionRef(std::move(coinbase));

  // Because we changed a transaction
  invalid_block->ComputeMerkleTrees();

  NewPoSValidBlockListener listener;

  BOOST_CHECK(!ProcessBlock(invalid_block));
  const auto it = listener.checked_blocks.find(invalid_block->GetHash());
  BOOST_REQUIRE(it != listener.checked_blocks.end());
  CValidationState vs = it->second;
  BOOST_CHECK_EQUAL(vs.GetRejectCode(), 64);
  BOOST_CHECK_EQUAL(vs.GetRejectReason(), "non-mandatory-script-verify-flag (Witness requires empty scriptSig)");
  BOOST_CHECK_EQUAL(0, listener.new_pos_valid_blocks.count(invalid_block->GetHash()));

  // Now submit valid block to ensure that it actually was valid before we tampered it
  BOOST_CHECK(ProcessBlock(valid_block));
  BOOST_CHECK_EQUAL(1, listener.new_pos_valid_blocks.count(valid_block->GetHash()));
}

BOOST_AUTO_TEST_SUITE_END()
