// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/blocktools.h>

#include <boost/test/unit_test.hpp>

namespace blocktools {

CBlockIndex *BlockIndexFake::MakeBlockIndex(const uint256 &hash, CBlockIndex *const prev) {
  auto result = block_indexes.emplace(hash, CBlockIndex());
  CBlockIndex *const index = &result.first->second;
  index->phashBlock = &result.first->first;
  index->nHeight = prev ? prev->nHeight + 1 : 0;
  index->pprev = prev;
  return index;
}

CBlockIndex *BlockIndexFake::Lookup(const uint256 &hash) {
  auto it = block_indexes.find(hash);
  BOOST_REQUIRE_MESSAGE(
      it != block_indexes.end(),
      "no block index known by this hash in this instance of BlockIndexFake.");
  return &it->second;
}

CBlockIndex *BlockIndexFake::Generate(const std::size_t count, const CBlockIndex *starting_point) {
  CBlockIndex *const starting_index = starting_point ? Lookup(starting_point->GetBlockHash())
                                                     : MakeBlockIndex(GetRandHash(), nullptr);
  BOOST_REQUIRE(starting_index);
  BOOST_REQUIRE(starting_index->phashBlock);
  CBlockIndex *current_index = starting_index;
  for (std::size_t i = starting_point ? 0 : 1; i < count; ++i) {
    const uint256 hash = GetRandHash();
    current_index = MakeBlockIndex(hash, current_index);
    BOOST_REQUIRE(current_index);
    BOOST_REQUIRE(current_index->phashBlock);
    BOOST_REQUIRE(current_index->pprev);
  }
  return current_index;
}

std::vector<CBlockIndex *> BlockIndexFake::GetChain(const CBlockIndex *tip) {
  BOOST_REQUIRE(tip != nullptr);
  auto it = block_indexes.find(tip->GetBlockHash());
  BOOST_REQUIRE_MESSAGE(
      it != block_indexes.end(),
      "tip not known by this instance of BlockIndexFake.");
  std::vector<CBlockIndex *> result;
  CBlockIndex *found_tip = &it->second;
  result.resize(tip->nHeight + 1, nullptr);
  for (CBlockIndex *walk = found_tip; walk != nullptr; walk = walk->pprev) {
    result[walk->nHeight] = walk;
  }
  return result;
}

void BlockIndexFake::SetupActiveChain(const CBlockIndex *tip,
                                      mocks::ActiveChainMock &active_chain_mock) {
  BOOST_REQUIRE(tip != nullptr);
  auto active_chain = std::make_shared<std::vector<CBlockIndex *>>(GetChain(tip));
  active_chain_mock.mock_GetSize.SetStub([active_chain]() { return active_chain->size(); });
  active_chain_mock.mock_GetHeight.SetStub([active_chain]() { return active_chain->size() - 1; });
  active_chain_mock.mock_GetDepth.SetStub([active_chain](const blockchain::Height height) {
    return active_chain->size() - height;
  });
  active_chain_mock.mock_AtHeight.SetStub([active_chain](const blockchain::Height height) -> const CBlockIndex * {
    if (height >= active_chain->size()) {
      return nullptr;
    }
    return active_chain->at(height);
  });
  active_chain_mock.mock_GetTip.SetStub([active_chain]() {
    BOOST_REQUIRE_MESSAGE(
        !active_chain->empty(),
        "GetTip() called on an empty chain (this is probably an error in mocking, "
        "an active chain should at least always contain a genesis block).");
    return active_chain->back();
  });
  active_chain_mock.mock_GetGenesis.SetStub([active_chain]() {
    BOOST_REQUIRE_MESSAGE(
        !active_chain->empty(),
        "GetGenesis() called on an empty chain (this is probably an error in mocking, "
        "an active chain should at least always contain a genesis block).");
    return active_chain->at(0);
  });
  active_chain_mock.mock_Contains.SetStub([active_chain](const CBlockIndex &block_index) {
    if (block_index.nHeight >= active_chain->size()) {
      return false;
    }
    const CBlockIndex *at_height = active_chain->at(block_index.nHeight);
    return at_height->GetBlockHash() == block_index.GetBlockHash();
  });
  active_chain_mock.mock_FindForkOrigin.SetStub([&active_chain_mock](const CBlockIndex &block_index) {
    const CBlockIndex *walk = &block_index;
    while (walk != nullptr && active_chain_mock.AtHeight(walk->nHeight) != walk) {
      walk = walk->pprev;
    }
    return walk;
  });
  active_chain_mock.mock_GetNext.SetStub([&active_chain_mock](const CBlockIndex &block_index) -> const CBlockIndex * {
    if (active_chain_mock.AtHeight(block_index.nHeight) == &block_index) {
      return active_chain_mock.AtHeight(block_index.nHeight + 1);
    }
    return nullptr;
  });
}

}  // namespace blocktools
