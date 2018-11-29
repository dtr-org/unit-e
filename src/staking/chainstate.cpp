// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/chainstate.h>

#include <chainparams.h>
#include <timedata.h>
#include <util.h>
#include <validation.h>

namespace staking {

class ChainStateAdapter final : public ChainState {

 public:
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

  uint256 GetTipUTXOSetHash() const override {
    // UNIT-E stub: requires kostia's implementation, tip->GetSnapshotHash()-> yada
    // yada yada
    return uint256();
  }

 private:
  bool ProcessNewBlock(std::shared_ptr<const CBlock> pblock) override {
    bool newBlock;
    return ::ProcessNewBlock(::Params().BlockchainParameters(), pblock, true, &newBlock);
  }

  const CChainParams &GetChainParams() const override { return ::Params(); }

  ::SyncStatus GetInitialBlockDownloadStatus() const override {
    return ::GetInitialBlockDownloadStatus();
  }
};

std::unique_ptr<ChainState> ChainState::New() {
  return std::unique_ptr<ChainState>(new ChainStateAdapter());
}

}  // namespace staking
