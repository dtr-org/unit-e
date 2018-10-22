// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/chainstate.h>

#include <chainparams.h>
#include <timedata.h>
#include <util.h>
#include <validation.h>

namespace proposer {

class ChainStateAdapter final : public ChainState {

  CCriticalSection &GetLock() const override { return cs_main; }

  uint32_t GetHeight() const override {
    const int height = chainActive.Height();
    if (height < 0) {
      throw std::runtime_error("no active chain yet");
    }
    return static_cast<uint32_t>(height);
  }

  std::unique_ptr<const CBlockHeader> GetTip() const override {
    const CBlockIndex *tip = chainActive.Tip();
    if (!tip) {
      throw std::runtime_error("no active chain yet");
    }
    return MakeUnique<const CBlockHeader>(tip->GetBlockHeader());
  }

  bool ProcessNewBlock(std::shared_ptr<const CBlock> pblock) override {
    bool newBlock;
    return ::ProcessNewBlock(::Params(), pblock, true, &newBlock);
  }

  const CChainParams &GetChainParams() const override { return ::Params(); }

  ::SyncStatus GetInitialBlockDownloadStatus() const override {
    return ::GetInitialBlockDownloadStatus();
  }
};

std::unique_ptr<ChainState> ChainState::MakeChain() {
  return std::unique_ptr<ChainState>(new ChainStateAdapter());
}

}  // namespace proposer
