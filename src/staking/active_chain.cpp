// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/active_chain.h>

#include <chainparams.h>
#include <coins.h>
#include <timedata.h>
#include <util/system.h>
#include <validation.h>

namespace staking {

class ActiveChainAdapter final : public ActiveChain {

 public:
  ActiveChainAdapter() = default;

  CCriticalSection &GetLock() const override { return cs_main; }

  const CBlockIndex *GetTip() const override {
    return chainActive.Tip();
  }

  const CBlockIndex *GetGenesis() const override {
    return chainActive.Genesis();
  }

  bool Contains(const CBlockIndex &block_index) const override {
    return chainActive.Contains(&block_index);
  }

  const CBlockIndex *FindForkOrigin(const CBlockIndex &fork) const override {
    return chainActive.FindFork(&fork);
  }

  const CBlockIndex *GetNext(const CBlockIndex &block_index) const override {
    return chainActive.Next(&block_index);
  }

  blockchain::Height GetSize() const override {
    return GetHeight() + 1;
  }

  blockchain::Height GetHeight() const override {
    // prevent returning negative numbers which would be turned into extremely big unsigned numbers
    const int height = chainActive.Height();
    if (height < 0) {
      LogPrintf("ERROR: Genesis block not loaded yet (got height=%d in %s)\n", height, __func__);
      throw std::runtime_error("genesis block not loaded yet");
    }
    return static_cast<blockchain::Height>(height);
  }

  const CBlockIndex *AtDepth(const blockchain::Depth depth) const override {
    return AtHeight(GetSize() - depth);
  }

  const CBlockIndex *AtHeight(const blockchain::Height height) const override {
    return chainActive[height];
  }

  const CBlockIndex *GetBlockIndex(const uint256 &block_hash) const override {
    AssertLockHeld(GetLock());
    const CBlockIndex *const block_index = LookupBlockIndex(block_hash);
    if (!block_index) {
      return nullptr;
    }
    if (!Contains(*block_index)) {
      // the block is not part of the active chain but in a fork.
      return nullptr;
    }
    return block_index;
  }

  blockchain::Depth GetDepth(const blockchain::Height height) const override {
    // Depth is relative to the currently active chain's height.
    // The depth of the tip of the chain is one by definition.
    // Depth zero does not exist.
    // The genesis block is at height=0 and has a depth=1.
    return GetHeight() - height + 1;
  }

  const uint256 ComputeSnapshotHash() const override {
    AssertLockHeld(GetLock());
    return pcoinsTip->GetSnapshotHash().GetHash(*GetTip());
  }

  bool ProposeBlock(std::shared_ptr<const CBlock> pblock) override {
    return ::ProcessNewBlock(::Params(), pblock, /* fForceProcessing= */ true, nullptr);
  }

  ::SyncStatus GetInitialBlockDownloadStatus() const override {
    return ::GetInitialBlockDownloadStatus();
  }

  boost::optional<Coin> GetUTXO(const COutPoint &out_point) const override {
    AssertLockHeld(GetLock());
    const ::Coin &coin = pcoinsTip->AccessCoin(out_point);
    if (coin.IsSpent()) {
      return boost::none;
    }
    const CBlockIndex *const block = AtHeight(coin.nHeight);
    if (!block) {
      return boost::none;
    }
    return Coin(block, out_point, coin.out);
  }
};

std::unique_ptr<ActiveChain> ActiveChain::New() {
  return std::unique_ptr<ActiveChain>(new ActiveChainAdapter());
}

}  // namespace staking
