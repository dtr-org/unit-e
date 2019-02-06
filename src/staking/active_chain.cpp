// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/active_chain.h>

#include <chainparams.h>
#include <timedata.h>
#include <util.h>
#include <validation.h>
#include "coin.h"

namespace staking {

class ActiveChainAdapter final : public ActiveChain {

 public:
  explicit ActiveChainAdapter() = default;

  CCriticalSection &GetLock() const override { return cs_main; }

  const CBlockIndex *GetTip() const override {
    return chainActive.Tip();
  }

  blockchain::Height GetSize() const override {
    return static_cast<blockchain::Height>(chainActive.Height() + 1);
  }

  blockchain::Height GetHeight() const override {
    return static_cast<blockchain::Height>(chainActive.Height());
  }

  const CBlockIndex *AtDepth(const blockchain::Depth depth) override {
    return AtHeight(GetSize() - depth);
  }

  const CBlockIndex *AtHeight(const blockchain::Height height) override {
    return chainActive[height];
  }

  const CBlockIndex *GetBlockIndex(const uint256 &block_hash) const override {
    const CBlockIndex *const block_index = LookupBlockIndex(block_hash);
    if (!block_index) {
      return nullptr;
    }
    if (chainActive[block_index->nHeight] != block_index) {
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
    return pcoinsTip->GetSnapshotHash().GetHash(*GetTip());
  }

  bool ProcessNewBlock(std::shared_ptr<const CBlock> pblock) override {
    return false;
    //    bool newBlock;
    //    return ::ProcessNewBlock(::Params(), pblock, true, &newBlock);
  }

  ::SyncStatus GetInitialBlockDownloadStatus() const override {
    return ::GetInitialBlockDownloadStatus();
  }

  boost::optional<Coin> GetUTXO(const COutPoint &outPoint) const override {
    AssertLockHeld(GetLock());
    const ::Coin &coin = pcoinsTip->AccessCoin(outPoint);
    if (coin.IsSpent()) {
      return boost::none;
    }
    Coin result_coin;
    result_coin.txid = outPoint.hash;
    result_coin.index = outPoint.n;
    result_coin.amount = coin.out.nValue;
    result_coin.script_pubkey = coin.out.scriptPubKey;
    result_coin.depth = GetDepth(coin.nHeight);
    return result_coin;
  }
};

std::unique_ptr<ActiveChain> ActiveChain::New() {
  return std::unique_ptr<ActiveChain>(new ActiveChainAdapter());
}

}  // namespace staking
