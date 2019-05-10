// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_TEST_UTIL_BLOCKTOOLS_H
#define UNIT_E_TEST_UTIL_BLOCKTOOLS_H

#include <test/test_unite_mocks.h>

#include <chain.h>
#include <random.h>
#include <uint256.h>

#include <map>

namespace blocktools {

struct BlockIndexFake {

  std::map<uint256, CBlockIndex> block_indexes;

  //! Creates a new CBlockIndex in block_indexes and returns a pointer to it.
  //!
  //! The height and pointer to previous block are deduced from the previous
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
  CBlockIndex *Generate(std::size_t count, const uint256 &starting_point = uint256::zero);

  //! Retrieves a chain that ends in the specified tip.
  std::shared_ptr<std::vector<CBlockIndex *>> GetChain(const uint256 &tip_hash);

  //! Stubs an active chain mock with stubs that use the block index from this
  //! instance and activates the chain which has the given tip.
  void SetupActiveChain(const CBlockIndex *tip,
                        mocks::ActiveChainMock &active_chain_mock);
};

}  // namespace blocktools

#endif  //UNIT_E_TEST_UTIL_BLOCKTOOLS_H
