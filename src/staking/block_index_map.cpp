// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include<staking/block_index_map.h>
#include<validation.h>

namespace staking {
namespace {

class BlockIndexMapImpl final : public BlockIndexMap {
public:
  CCriticalSection &GetLock() const override {
    return cs_main;
  }

  CBlockIndex *Lookup(const uint256 &block_hash) const override{
    AssertLockHeld(GetLock());
    return LookupBlockIndex(block_hash);
  }

  void ForEach(std::function<bool(const uint256 &, const CBlockIndex &)> &&f) const override {
    AssertLockHeld(GetLock());
    for (auto it = mapBlockIndex.begin(); it != mapBlockIndex.end(); ++it) {
      if (!f(it->first, *it->second)) {
        return;
      }
    }
  }
};

}

std::unique_ptr<BlockIndexMap> BlockIndexMap::New() {
  return MakeUnique<BlockIndexMapImpl>();
}

}
