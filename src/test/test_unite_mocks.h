// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_TEST_UNITE_MOCKS_H
#define UNIT_E_TEST_UNITE_MOCKS_H

#include <staking/active_chain.h>
#include <staking/network.h>
#include <staking/stake_validator.h>

#include <atomic>
#include <cstdint>
#include <functional>

namespace mocks {

class NetworkMock : public staking::Network {
 public:
  mutable std::atomic<std::uint32_t> invocations_GetTime;
  mutable std::atomic<std::uint32_t> invocations_GetNodeCount;
  mutable std::atomic<std::uint32_t> invocations_GetInboundNodeCount;
  mutable std::atomic<std::uint32_t> invocations_GetOutboundNodeCount;

  std::int64_t time = 0;
  size_t node_count = 0;
  size_t inbound_node_count = 0;
  size_t outbound_node_count = 0;

  int64_t GetTime() const override {
    ++invocations_GetTime;
    return time;
  }
  size_t GetNodeCount() const override {
    ++invocations_GetNodeCount;
    return node_count;
  }
  size_t GetInboundNodeCount() const override {
    ++invocations_GetInboundNodeCount;
    return inbound_node_count;
  }
  size_t GetOutboundNodeCount() const override {
    ++invocations_GetOutboundNodeCount;
    return outbound_node_count;
  }
};

class ActiveChainMock : public staking::ActiveChain {
  mutable CCriticalSection lock;

 public:
  mutable std::atomic<std::uint32_t> invocations_GetLock;
  mutable std::atomic<std::uint32_t> invocations_GetSize;
  mutable std::atomic<std::uint32_t> invocations_GetHeight;
  mutable std::atomic<std::uint32_t> invocations_GetTip;
  mutable std::atomic<std::uint32_t> invocations_AtDepth;
  mutable std::atomic<std::uint32_t> invocations_AtHeight;
  mutable std::atomic<std::uint32_t> invocations_GetDepth;
  mutable std::atomic<std::uint32_t> invocations_GetBlockIndex;
  mutable std::atomic<std::uint32_t> invocations_ComputeSnapshotHash;
  mutable std::atomic<std::uint32_t> invocations_ProcessNewBlock;
  mutable std::atomic<std::uint32_t> invocations_GetUTXO;
  mutable std::atomic<std::uint32_t> invocations_GetInitialBlockDownloadStatus;

  ActiveChainMock()
      : invocations_GetLock(0),
        invocations_GetSize(0),
        invocations_GetHeight(0),
        invocations_GetTip(0),
        invocations_AtDepth(0),
        invocations_AtHeight(0),
        invocations_GetDepth(0),
        invocations_GetBlockIndex(0),
        invocations_ComputeSnapshotHash(0),
        invocations_ProcessNewBlock(0),
        invocations_GetUTXO(0),
        invocations_GetInitialBlockDownloadStatus(0) {}

  //! The tip to be returned by GetTip()
  CBlockIndex *tip = nullptr;

  //! The sync states to be returned by GetIBDStatus()
  ::SyncStatus sync_status = SyncStatus::SYNCED;

  //! The height to be returned by GetHeight() (GetSize = GetHeight + 1)
  blockchain::Height height = 0;

  //! The snapshot hash to be returned by ComputeSnapshotHash()
  uint256 snapshot_hash = uint256();

  //! Function to retrieve the block at the given depth
  std::function<CBlockIndex *(blockchain::Depth)> block_at_depth = [](blockchain::Depth) {
    return nullptr;
  };

  //! Function to retrieve the block at the given height
  std::function<CBlockIndex *(blockchain::Height)> block_at_height = [](blockchain::Height) {
    return nullptr;
  };

  //! Function to retrieve the block at the given index
  std::function<CBlockIndex *(const uint256 &)> get_block_index = [](const uint256 &) {
    return nullptr;
  };

  //! Function to retrieve the block at the given index
  std::function<boost::optional<staking::Coin>(const COutPoint &)> get_utxo = [](const COutPoint &) {
    return boost::none;
  };

  CCriticalSection &GetLock() const override {
    ++invocations_GetLock;
    return lock;
  }
  blockchain::Height GetSize() const override {
    ++invocations_GetSize;
    return height + 1;
  }
  blockchain::Height GetHeight() const override {
    ++invocations_GetHeight;
    return height;
  }
  const CBlockIndex *GetTip() const override {
    ++invocations_GetTip;
    return tip;
  }
  const CBlockIndex *AtDepth(blockchain::Depth depth) override {
    ++invocations_AtDepth;
    return block_at_depth(depth);
  }
  const CBlockIndex *AtHeight(blockchain::Height height) override {
    ++invocations_AtHeight;
    return block_at_height(height);
  }
  blockchain::Depth GetDepth(const blockchain::Height) const override {
    ++invocations_GetDepth;
    return GetHeight() - height + 1;
  }
  const CBlockIndex *GetBlockIndex(const uint256 &hash) const override {
    ++invocations_GetBlockIndex;
    return get_block_index(hash);
  }
  const uint256 ComputeSnapshotHash() const override {
    ++invocations_ComputeSnapshotHash;
    return snapshot_hash;
  }
  bool ProcessNewBlock(std::shared_ptr<const CBlock> pblock) override {
    ++invocations_ProcessNewBlock;
    return false;
  }
  boost::optional<staking::Coin> GetUTXO(const COutPoint &outpoint) const override {
    ++invocations_GetUTXO;
    return get_utxo(outpoint);
  }
  ::SyncStatus GetInitialBlockDownloadStatus() const override {
    ++invocations_GetInitialBlockDownloadStatus;
    return sync_status;
  }
};

class StakeValidatorMock : public staking::StakeValidator {
  mutable CCriticalSection lock;

 public:
  std::function<bool(uint256)> checkkernelfunc =
      [](uint256 kernel) { return false; };
  std::function<uint256(const CBlockIndex *, const staking::Coin &, blockchain::Time)> computekernelfunc =
      [](const CBlockIndex *, const staking::Coin &, blockchain::Time) { return uint256(); };

  CCriticalSection &GetLock() override {
    return lock;
  }
  bool CheckKernel(CAmount, const uint256 &kernel, blockchain::Difficulty) const override {
    return checkkernelfunc(kernel);
  }
  uint256 ComputeKernelHash(const CBlockIndex *blockindex, const staking::Coin &coin, blockchain::Time time) const override {
    return computekernelfunc(blockindex, coin, time);
  }
  staking::BlockValidationResult CheckStake(const CBlock &) const override {
    return staking::BlockValidationResult();
  }
  uint256 ComputeStakeModifier(const CBlockIndex *, const uint256 &) const override { return uint256(); }
  bool IsPieceOfStakeKnown(const COutPoint &) const override { return false; }
  void RememberPieceOfStake(const COutPoint &) override {}
  void ForgetPieceOfStake(const COutPoint &) override {}
};

}  // namespace mocks

#endif  //UNIT_E_TEST_UNITE_MOCKS_H
