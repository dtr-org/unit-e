// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_STAKING_BLOCK_INDEX_MAP_H
#define UNITE_STAKING_BLOCK_INDEX_MAP_H

#include <sync.h>
#include <functional>
#include <memory>

class CBlockIndex;
class uint256;

namespace staking {

//! \brief An interface to the current storage of CBlockIndex-es.
//!
//! Wrapper on mapBlockIndex.
class BlockIndexMap {
 public:
  //! \brief Returns the mutex that protects map.
  virtual CCriticalSection &GetLock() const = 0;

  //! \brief Lookups block index in the map.
  virtual CBlockIndex *Lookup(const uint256 &block_hash) const = 0;

  virtual void ForEach(std::function<bool(const uint256 &, const CBlockIndex &)> &&) const = 0;

  virtual ~BlockIndexMap() = default;

  static std::unique_ptr<BlockIndexMap> New();
};

}  // namespace staking

#endif  // UNITE_STAKING_BLOCK_INDEX_MAP_H
