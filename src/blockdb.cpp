// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockdb.h>

#include <chainparams.h>
#include <validation.h>

//! Implementation of BlockDB that uses disk to save and read the block
//! data. It delegates to bitcoin functions like `ReadBlockFromDisk`.
class BlockDiskStorage final : public BlockDB {

 public:
  ~BlockDiskStorage() override = default;

  boost::optional<CBlock> ReadBlock(const CBlockIndex &index) override {
    boost::optional<CBlock> block((CBlock()));
    if (!ReadBlockFromDisk(*block, &index)) {
      return boost::none;
    }
    return block;
  }
};

std::unique_ptr<BlockDB> BlockDB::New() {
  return std::unique_ptr<BlockDB>(new BlockDiskStorage());
}
