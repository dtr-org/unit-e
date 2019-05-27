// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

 protected:
  void NewPoSValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock> &block) override {
    new_pos_valid_blocks.emplace(block->GetHash());
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

  const CBlock invalid_block = CreateAndProcessBlock({}, coinbase_script, stake, &processed);
  BOOST_CHECK(!processed);
  BOOST_CHECK_EQUAL(0, listener.new_pos_valid_blocks.count(invalid_block.GetHash()));
}

BOOST_AUTO_TEST_SUITE_END()
