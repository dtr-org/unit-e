// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/active_chain.h>

#include <chainparams.h>
#include <timedata.h>
#include <util.h>
#include <validation.h>

namespace staking {

class ActiveChainAdapter final : public ActiveChain {

 private:
  Dependency<blockchain::Behavior> m_blockchain_behavior;

 public:
  explicit ActiveChainAdapter(Dependency<blockchain::Behavior> blockchain_behavior)
      : m_blockchain_behavior(blockchain_behavior) {}

  CCriticalSection &GetLock() const override { return cs_main; }

  const CBlockIndex *GetTip() const override {
    return chainActive.Tip();
  };

  blockchain::Height GetSize() const override {
    return static_cast<blockchain::Height>(chainActive.Height() + 1);
  }

  blockchain::Height GetHeight() const override {
    return static_cast<blockchain::Height>(chainActive.Height());
  }

  const CBlockIndex *AtDepth(blockchain::Depth depth) override {
    return AtHeight(GetSize() - depth);
  }

  const CBlockIndex *AtHeight(blockchain::Height height) override {
    return chainActive[height];
  }

  const uint256 ComputeSnapshotHash() const override {
    return pcoinsTip->GetSnapshotHash().GetHash(GetTip()->stake_modifier);
  }

  bool ProcessNewBlock(std::shared_ptr<const CBlock> pblock) override {
    return false;
//    bool newBlock;
//    return ::ProcessNewBlock(::Params(), pblock, true, &newBlock);
  }

  ::SyncStatus GetInitialBlockDownloadStatus() const override {
    return ::GetInitialBlockDownloadStatus();
  }
};

std::unique_ptr<ActiveChain> ActiveChain::New(
    Dependency<blockchain::Behavior> blockchain_behavior) {
  return std::unique_ptr<ActiveChain>(new ActiveChainAdapter(blockchain_behavior));
}

}  // namespace staking
