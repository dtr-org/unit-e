// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/blocktools.h>

#include <random.h>
#include <uint256.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(blocktools_tests)

BOOST_AUTO_TEST_CASE(BLockIndexFake_MakeBlockIndex) {

  blocktools::BlockIndexFake fake;

  BOOST_CHECK_EQUAL(fake.block_indexes.size(), 0);

  {
    // create a block at height=0
    const uint256 hash = GetRandHash();
    CBlockIndex *block_index = fake.MakeBlockIndex(hash, nullptr);

    BOOST_CHECK_EQUAL(fake.block_indexes.size(), 1);
    BOOST_CHECK_EQUAL(&fake.block_indexes.begin()->second, block_index);
    BOOST_CHECK_EQUAL(block_index->nHeight, 0);
    BOOST_CHECK(block_index->pprev == nullptr);
    BOOST_REQUIRE(block_index->phashBlock);
    BOOST_CHECK_EQUAL(*block_index->phashBlock, hash);
  }

  {
    // create a successor to the previously generated block
    const uint256 hash = GetRandHash();
    CBlockIndex *block_index = fake.MakeBlockIndex(hash, &fake.block_indexes.begin()->second);

    BOOST_CHECK_EQUAL(fake.block_indexes.size(), 2);
    BOOST_CHECK_EQUAL(block_index->nHeight, 1);
    BOOST_CHECK(block_index->pprev);
    BOOST_REQUIRE(block_index->phashBlock);
    BOOST_CHECK_EQUAL(*block_index->phashBlock, hash);
  }
}

BOOST_AUTO_TEST_CASE(BlockIndexFake_Generate) {

  blocktools::BlockIndexFake fake;

  BOOST_CHECK_EQUAL(fake.block_indexes.size(), 0);
  CBlockIndex *tip = fake.Generate(5);
  BOOST_CHECK_EQUAL(fake.block_indexes.size(), 5);

  // the blocks are at height=0 till height=4 which makes for 5 blocks
  std::size_t count = 0;
  std::size_t height = 4;
  while (tip) {
    BOOST_CHECK(tip->phashBlock);
    BOOST_CHECK_EQUAL(tip->nHeight, height);
    tip = tip->pprev;
    --height;
    ++count;
  }
  BOOST_CHECK_EQUAL(count, 5);
}

BOOST_AUTO_TEST_CASE(BlockIndexFake_GetChain) {

  blocktools::BlockIndexFake fake;

  const CBlockIndex *tip = fake.Generate(10);
  BOOST_REQUIRE(tip);
  const std::vector<CBlockIndex *> chain = fake.GetChain(tip->GetBlockHash());

  BOOST_REQUIRE_EQUAL(chain.size(), 10);

  const CBlockIndex *previous = nullptr;
  for (std::size_t height = 0; height < 10; ++height) {
    const CBlockIndex *const block_index = chain.at(height);

    BOOST_REQUIRE(block_index);
    BOOST_CHECK(block_index->phashBlock);
    BOOST_CHECK_EQUAL(block_index->nHeight, height);
    BOOST_CHECK_EQUAL(block_index->pprev, previous);

    previous = block_index;
  }
}

BOOST_AUTO_TEST_CASE(BlockIndexFake_Feature_Forks) {

  blocktools::BlockIndexFake fake;

  BOOST_CHECK_EQUAL(fake.block_indexes.size(), 0);

  const CBlockIndex *tip1 = fake.Generate(10);
  BOOST_CHECK_EQUAL(fake.block_indexes.size(), 10);

  std::vector<CBlockIndex *> chain1 = fake.GetChain(tip1->GetBlockHash());
  BOOST_CHECK_EQUAL(chain1.size(), 10);

  {
    const CBlockIndex *previous = nullptr;
    for (std::size_t height = 0; height < chain1.size(); ++height) {
      const CBlockIndex *const block_index = chain1.at(height);

      BOOST_REQUIRE(block_index);
      BOOST_CHECK(block_index->phashBlock);
      BOOST_CHECK_EQUAL(block_index->nHeight, height);
      BOOST_CHECK_EQUAL(block_index->pprev, previous);

      previous = block_index;
    }
    BOOST_CHECK_EQUAL(previous, tip1);
  }

  const CBlockIndex *tip2 = fake.Generate(3, chain1.at(5)->GetBlockHash());
  // 10 blocks previously, plus an additional 3
  BOOST_CHECK_EQUAL(fake.block_indexes.size(), 13);

  std::vector<CBlockIndex *> chain2 = fake.GetChain(tip2->GetBlockHash());
  // at height=5 there is the 6th block, plus 3 on top
  BOOST_CHECK_EQUAL(chain2.size(), 9);

  {
    const CBlockIndex *previous = nullptr;
    for (std::size_t height = 0; height < chain2.size(); ++height) {
      const CBlockIndex *const block_index = chain2.at(height);

      BOOST_REQUIRE(block_index);
      BOOST_CHECK(block_index->phashBlock);
      BOOST_CHECK_EQUAL(block_index->nHeight, height);
      BOOST_CHECK_EQUAL(block_index->pprev, previous);

      previous = block_index;
    }
    BOOST_CHECK_EQUAL(previous, tip2);
  }
}

BOOST_AUTO_TEST_SUITE_END()
