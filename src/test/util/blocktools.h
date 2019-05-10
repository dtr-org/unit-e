// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_TEST_UTIL_BLOCKTOOLS_H
#define UNIT_E_TEST_UTIL_BLOCKTOOLS_H

#include <test/test_unite_mocks.h>

#include <chain.h>
#include <uint256.h>

#include <cstdint>
#include <map>

namespace blocktools {

struct BlockIndexFake {

  std::map<uint256, CBlockIndex> block_indexes;

  //! Creates a new CBlockIndex in block_indexes and returns a pointer to it.
  //!
  //! The height and pointer to the previous block are deduced from the previous
  //! block index. If none is given (nullptr) a block at height 0 without a
  //! predecessor is created.
  //!
  //! \param hash The block hash for the new block index
  //! \param prev The previous block (maybe nullptr)
  //!
  //! \return A pointer to the newly created block index.
  CBlockIndex *MakeBlockIndex(const uint256 &hash, CBlockIndex *prev);

  //! Generates a bunch of CBlockIndexes that form a chain.
  //!
  //! The chain maybe a fork, when a starting_point is given.
  //!
  //! \param count The number of blocks to be created.
  //! \param starting_point optional, the starting point to fork from.
  //!
  //! \return A pointer to the tip of the newly created chain.
  CBlockIndex *Generate(std::size_t count, const CBlockIndex *starting_point = nullptr);

  //! Generates a random hash and encodes height and fork_number in it.
  //!
  //! Example hashes:
  //! - 2046b80afe8458145d02244c8958b5e000000000000000000000000000000000
  //!                                   ^ fork=0        ^ height=0
  //! - 37cde99f37f4ee323ab10afc8c6e5fa300000000000000020000000000000007
  //!                                   ^ fork=2        ^ height=7
  //! - ef101be55d91aa5adfe8df797432fbb5ffffffffffffffff00000000008da8a1
  //!                                   fork=uint64_max ^ height=9283745
  uint256 GenerateHash(std::uint64_t height, std::uint64_t fork_number) const;

  //! Retrieves a chain that ends in the specified tip.
  std::vector<CBlockIndex *> GetChain(const CBlockIndex *tip);

  //! Looks up a node by id which is known to exist in this block index.
  //! Fails with BOOST_REQUIRE_MESSAGE if it does not exist.
  CBlockIndex *Lookup(const uint256 &hash);

  //! Stubs an active chain mock with stubs that use the block index from this
  //! instance and activates the chain which has the given tip.
  void SetupActiveChain(const CBlockIndex *tip,
                        mocks::ActiveChainMock &active_chain_mock);

 private:
  std::size_t number_of_forks = 0;
};

}  // namespace blocktools

#endif  //UNIT_E_TEST_UTIL_BLOCKTOOLS_H
