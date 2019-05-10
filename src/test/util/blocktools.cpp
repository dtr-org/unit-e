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

CBlockIndex *BlockIndexFake::Generate(const std::size_t count, const uint256 &starting_point) {
  auto starting_block = block_indexes.find(starting_point);
  std::size_t height = 0;
  CBlockIndex *const starting_index = [&]() {
    if (starting_block == block_indexes.end()) {
      ++height;
      return MakeBlockIndex(starting_point, nullptr);
    } else {
      return &starting_block->second;
    }
  }();
  BOOST_REQUIRE(starting_index);
  BOOST_REQUIRE(starting_index->phashBlock);

  CBlockIndex *current_index = starting_index;
  for (; height < count; ++height) {
    const uint256 hash = GetRandHash();
    current_index = MakeBlockIndex(hash, current_index);
    BOOST_REQUIRE(current_index);
    BOOST_REQUIRE(current_index->phashBlock);
    BOOST_REQUIRE(current_index->pprev);
  }

  return current_index;
}

std::shared_ptr<std::vector<CBlockIndex *>> BlockIndexFake::GetChain(const uint256 &tip_hash) {
  auto tip = block_indexes.find(tip_hash);
  auto result = std::make_shared<std::vector<CBlockIndex *>>();
  if (tip == block_indexes.end()) {
    return result;
  }
  CBlockIndex *current = &tip->second;
  result->resize(current->nHeight + 1, nullptr);
  std::size_t ix = current->nHeight;
  do {
    (*result)[ix] = current;
    current = current->pprev;
  } while (ix-- != 0);
  BOOST_REQUIRE(current == nullptr);
  return result;
}

void BlockIndexFake::SetupActiveChain(const CBlockIndex *tip,
                                      mocks::ActiveChainMock &active_chain_mock) {
  assert(tip != nullptr);
  std::shared_ptr<std::vector<CBlockIndex *>> active_chain = GetChain(tip->GetBlockHash());
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
    return active_chain->back();
  });
  active_chain_mock.mock_GetGenesis.SetStub([active_chain]() {
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
