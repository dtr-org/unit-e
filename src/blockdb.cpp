// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockdb.h>

#include <chainparams.h>
#include <validation.h>

//! \brief Implementation of BlockDB that uses disk to save and read the block
//! data. It delegates to bitcoin functions like `ReadBlockFromDisk`.
class BlockDiskStorage final : public BlockDB {

 private:
  Consensus::Params consensus_params;

 public:
  ~BlockDiskStorage() override = default;

  BlockDiskStorage() : consensus_params(Params().GetConsensus()) {}

  boost::optional<CBlock> ReadBlock(const CBlockIndex &index) override {
    boost::optional<CBlock> block{CBlock()};
    if (!ReadBlockFromDisk(*block, &index, consensus_params)) {
      return boost::none;
    } else {
      return block;
    }
  }
};

std::unique_ptr<BlockDB>
BlockDB::New() {
  return std::unique_ptr<BlockDB>(new BlockDiskStorage());
}
