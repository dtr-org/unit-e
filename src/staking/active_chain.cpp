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

  std::uint32_t GetSize() const override {
    return static_cast<std::uint32_t>(chainActive.Height() + 1);
  }

  const CBlockIndex *operator[](std::int64_t height) override {
    LogPrint(BCLog::PROPOSING, "Chain is %d\n", GetSize());
    LogPrint(BCLog::PROPOSING, "Looking for %d\n", height);
    if (height < 0) {
      height = GetSize() + height;
    }
    LogPrint(BCLog::PROPOSING, "Now Looking for %d\n", height);
    return chainActive[height];
  }

  bool ProcessNewBlock(std::shared_ptr<const CBlock> pblock) override {
    bool newBlock;
    return ::ProcessNewBlock(::Params(), pblock, true, &newBlock);
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
