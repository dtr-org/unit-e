// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/blocktools.h>

#include <random.h>
#include <uint256.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

#include <limits>

BOOST_AUTO_TEST_SUITE(blocktools_tests)

BOOST_AUTO_TEST_CASE(BlockIndexFake_MakeBlockIndex) {

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
  const std::vector<CBlockIndex *> chain = fake.GetChain(tip);

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

BOOST_AUTO_TEST_CASE(BlockIndexFake_GenerateHash) {

  blocktools::BlockIndexFake fake;

  auto check = [&](const std::uint64_t height, const std::uint64_t fork_number) {
    const uint256 hash = fake.GenerateHash(height, fork_number);
    // nota bene: GetUint64 takes the index of a byte
    BOOST_CHECK_EQUAL(hash.GetUint64(0), height);
    BOOST_CHECK_EQUAL(hash.GetUint64(1), fork_number);
  };

  // some interesting numbers: minimum, one, an arbitrary number, and maximum
  const std::vector<std::uint64_t> interesting_numbers = {
      0,
      1,
      2,
      9283745,
      std::numeric_limits<std::uint64_t>::max()
  };
  // check all combinations of numbers
  for (auto i : interesting_numbers) {
    for (auto j : interesting_numbers) {
      check(i, j);
    }
  }
}

BOOST_AUTO_TEST_CASE(BlockIndexFake_Feature_Forks) {

  blocktools::BlockIndexFake fake;

  BOOST_CHECK_EQUAL(fake.block_indexes.size(), 0);

  const CBlockIndex *tip1 = fake.Generate(10);
  BOOST_CHECK_EQUAL(fake.block_indexes.size(), 10);

  std::vector<CBlockIndex *> chain1 = fake.GetChain(tip1);
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

  const CBlockIndex *tip2 = fake.Generate(3, chain1.at(5));
  // 10 blocks previously, plus an additional 3
  BOOST_CHECK_EQUAL(fake.block_indexes.size(), 13);

  std::vector<CBlockIndex *> chain2 = fake.GetChain(tip2);
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

BOOST_AUTO_TEST_CASE(BlockIndexFake_Feature_DebuggableBlockHashes) {

  blocktools::BlockIndexFake fake;

  auto check = [](const std::vector<CBlockIndex *> &chain, const std::size_t fork_point, const std::size_t fork_number) {
    for (auto ix : chain) {
      BOOST_CHECK_EQUAL(ix->GetBlockHash().GetUint64(0), ix->nHeight);
      BOOST_CHECK_EQUAL(ix->GetBlockHash().GetUint64(1), ix->nHeight > fork_point ? fork_number : 0);
    }
  };

  const CBlockIndex *tip = fake.Generate(100);
  const std::vector<CBlockIndex *> active_chain = fake.GetChain(tip);
  check(active_chain, 0, 0);

  const CBlockIndex *fork1_tip = fake.Generate(40, active_chain.at(20));
  const std::vector<CBlockIndex *> fork1 = fake.GetChain(fork1_tip);
  check(fork1, 20, 1);

  const CBlockIndex *fork2_tip = fake.Generate(40, active_chain.at(60));
  const std::vector<CBlockIndex *> fork2 = fake.GetChain(fork2_tip);
  check(fork2, 60, 2);

  const CBlockIndex *fork3_tip = fake.Generate(40, active_chain.at(80));
  const std::vector<CBlockIndex *> fork3 = fake.GetChain(fork3_tip);
  check(fork3, 80, 3);
}

BOOST_AUTO_TEST_SUITE_END()
