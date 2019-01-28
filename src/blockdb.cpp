// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockdb.h>
#include <validation.h>
#include <chainparams.h>

//! \brief Implementation of BlockDB that uses disk to save and read the block
//! data. It delegates to bitcoin functions like `ReadBlockFromDisk`.
class BlockDiskStorage final : public BlockDB {

 private:
  Consensus::Params consensus_params;

 public:
  ~BlockDiskStorage() override = default;

  BlockDiskStorage() : consensus_params(Params().GetConsensus()){}

  bool ReadBlock(CBlock &block_out, const CBlockIndex *pindex) override {

    return ReadBlockFromDisk(block_out, pindex, consensus_params);
  }

};
std::unique_ptr<BlockDB>
BlockDB::New() {
  return std::unique_ptr<BlockDB>(new BlockDiskStorage());
}

