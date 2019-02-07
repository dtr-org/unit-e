// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKDB_UTILS_H
#define UNIT_E_BLOCKDB_UTILS_H

#include <blockdb.h>

class MockBlockDB final : public BlockDB {
 private:
  std::map<uint256, CBlock> m_block_map;

 public:

  explicit MockBlockDB(std::map<uint256, CBlock> &blockMap) : m_block_map(blockMap) {}

  ~MockBlockDB() override = default;

  boost::optional<CBlock> ReadBlock(const CBlockIndex &index) override {

    auto it = m_block_map.find(index.GetBlockHash());
    if ( it == m_block_map.end()) {
      return boost::none;
    }

    return it->second;
  }
  static std::unique_ptr<BlockDB> New(std::map<uint256, CBlock> &blockMap) {
    std::cout << blockMap.size() << std::endl;
    return std::unique_ptr<BlockDB>(new MockBlockDB(blockMap));
  }
};

#endif //UNIT_E_BLOCKDB_UTILS_H
