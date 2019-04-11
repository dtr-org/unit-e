// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_TEST_UNITE_MOCKS_H
#define UNIT_E_TEST_UNITE_MOCKS_H

#include <blockdb.h>
#include <coins.h>
#include <finalization/state_db.h>
#include <staking/active_chain.h>
#include <staking/block_index_map.h>
#include <staking/block_validator.h>
#include <staking/network.h>
#include <staking/stake_validator.h>
#include <util.h>

#include <atomic>
#include <cstdint>
#include <functional>

namespace mocks {

//! \brief An ArgsManager that can be initialized using a list of cli args.
//!
//! Usage:
//!   ArgsManagerMock argsman{ "-proposing=0", "-printtoconsole" };
//!
//! Uses std::initializer_list, so the curly braces are actually required.
class ArgsManagerMock : public ArgsManager {
 public:
  ArgsManagerMock(std::initializer_list<std::string> args) {
    const char **argv = new const char *[args.size() + 1];
    argv[0] = "executable-name";
    std::size_t i = 1;
    for (const std::string &arg : args) {
      argv[i++] = arg.c_str();
    }
    ParseParameters(static_cast<int>(i), argv);
    delete[] argv;
  }
};

class NetworkMock : public staking::Network {
 public:
  mutable std::atomic<std::uint32_t> invocations_GetTime{0};
  mutable std::atomic<std::uint32_t> invocations_GetNodeCount{0};
  mutable std::atomic<std::uint32_t> invocations_GetInboundNodeCount{0};
  mutable std::atomic<std::uint32_t> invocations_GetOutboundNodeCount{0};

  mutable std::int64_t result_GetTime = 0;
  mutable std::size_t result_GetNodeCount = 0;
  mutable std::size_t result_GetInboundNodeCount = 0;
  mutable std::size_t result_GetOutboundNodeCount = 0;

  int64_t GetTime() const override {
    ++invocations_GetTime;
    return result_GetTime;
  }
  std::size_t GetNodeCount() const override {
    ++invocations_GetNodeCount;
    return result_GetNodeCount;
  }
  std::size_t GetInboundNodeCount() const override {
    ++invocations_GetInboundNodeCount;
    return result_GetInboundNodeCount;
  }
  std::size_t GetOutboundNodeCount() const override {
    ++invocations_GetOutboundNodeCount;
    return result_GetOutboundNodeCount;
  }
};

class BlockIndexMapMock : public staking::BlockIndexMap {
public:
  bool reverse = false;

  CCriticalSection &GetLock() const override { return cs; }
  CBlockIndex *Insert(const uint256 &block_hash) {
    const auto result = indexes.emplace(block_hash, new CBlockIndex());
    CBlockIndex *index = result.first->second;
    const uint256 &hash = result.first->first;
    if (!result.second) {
      return index;
    }
    index->phashBlock = &hash;
    return index;
  }
  CBlockIndex *Lookup(const uint256 &block_hash) const override {
    const auto it = indexes.find(block_hash);
    if (it == indexes.end()) {
      return nullptr;
    }
    return it->second;
  }
  void ForEach(std::function<bool(const uint256 &, const CBlockIndex &)> &&f) const override {
    if (!reverse) {
      for (const auto &indexe : indexes) {
        if (!f(indexe.first, *indexe.second)) {
          return;
        }
      }
    } else {
      for (auto it = indexes.rbegin(); it != indexes.rend(); ++it) {
        if (!f(it->first, *it->second)) {
          return;
        }
      }
    }
  }
  ~BlockIndexMapMock() override {
    for (auto &i : indexes) {
      delete i.second;
    }
  }
private:
  mutable CCriticalSection cs;
  std::map<uint256, CBlockIndex*> indexes;
};

class ActiveChainMock : public staking::ActiveChain {
  mutable CCriticalSection lock;

 public:
  mutable std::atomic<std::uint32_t> invocations_GetLock{0};
  mutable std::atomic<std::uint32_t> invocations_GetSize{0};
  mutable std::atomic<std::uint32_t> invocations_GetHeight{0};
  mutable std::atomic<std::uint32_t> invocations_GetTip{0};
  mutable std::atomic<std::uint32_t> invocations_GetGenesis{0};
  mutable std::atomic<std::uint32_t> invocations_Contains{0};
  mutable std::atomic<std::uint32_t> invocations_FindForkOrigin{0};
  mutable std::atomic<std::uint32_t> invocations_GetNext{0};
  mutable std::atomic<std::uint32_t> invocations_AtDepth{0};
  mutable std::atomic<std::uint32_t> invocations_AtHeight{0};
  mutable std::atomic<std::uint32_t> invocations_GetDepth{0};
  mutable std::atomic<std::uint32_t> invocations_GetBlockIndex{0};
  mutable std::atomic<std::uint32_t> invocations_ComputeSnapshotHash{0};
  mutable std::atomic<std::uint32_t> invocations_ProcessNewBlock{0};
  mutable std::atomic<std::uint32_t> invocations_GetUTXO{0};
  mutable std::atomic<std::uint32_t> invocations_GetInitialBlockDownloadStatus{0};

  //! The tip to be returned by GetTip()
  CBlockIndex *tip = nullptr;

  //! The genesis to be returned by GetGenesis()
  CBlockIndex *genesis = nullptr;

  //! The sync states to be returned by GetIBDStatus()
  ::SyncStatus sync_status = SyncStatus::SYNCED;

  //! The height to be returned by GetHeight() (GetSize = GetHeight + 1)
  blockchain::Height height = 0;

  //! The snapshot hash to be returned by ComputeSnapshotHash()
  uint256 snapshot_hash = uint256();

  //! Function to retrieve the block at the given depth
  std::function<CBlockIndex *(blockchain::Depth)> stub_AtDepth = [](blockchain::Depth) {
    return nullptr;
  };

  //! Function to retrieve the block at the given height
  std::function<CBlockIndex *(blockchain::Height)> stub_AtHeight = [](blockchain::Height) {
    return nullptr;
  };

  //! Function to retrieve the block at the given index
  std::function<CBlockIndex *(const uint256 &)> stub_GetBlockIndex = [](const uint256 &) {
    return nullptr;
  };

  //! Function to retrieve the block at the given index
  std::function<boost::optional<staking::Coin>(const COutPoint &)> stub_GetUTXO = [](const COutPoint &) {
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
  const CBlockIndex *GetGenesis() const override {
    ++invocations_GetGenesis;
    return genesis;
  }
  bool Contains(const CBlockIndex &block_index) const override {
    ++invocations_Contains;
    return stub_AtHeight(block_index.nHeight) == &block_index;
  }
  const CBlockIndex *FindForkOrigin(const CBlockIndex &block_index) const override {
    ++invocations_FindForkOrigin;
    const CBlockIndex *walk = &block_index;
    while (walk != nullptr && stub_AtHeight(walk->nHeight) != walk) {
      walk = walk->pprev;
    }
    return walk;
  }
  const CBlockIndex *GetNext(const CBlockIndex &block_index) const override {
    ++invocations_GetNext;
    if (stub_AtHeight(block_index.nHeight) == &block_index) {
      return stub_AtHeight(block_index.nHeight + 1);
    }
    return nullptr;
  }
  const CBlockIndex *AtDepth(blockchain::Depth depth) const override {
    ++invocations_AtDepth;
    return stub_AtDepth(depth);
  }
  const CBlockIndex *AtHeight(blockchain::Height height) const override {
    ++invocations_AtHeight;
    return stub_AtHeight(height);
  }
  blockchain::Depth GetDepth(const blockchain::Height height) const override {
    ++invocations_GetDepth;
    return GetHeight() - height + 1;
  }
  const CBlockIndex *GetBlockIndex(const uint256 &hash) const override {
    ++invocations_GetBlockIndex;
    return stub_GetBlockIndex(hash);
  }
  const uint256 ComputeSnapshotHash() const override {
    ++invocations_ComputeSnapshotHash;
    return snapshot_hash;
  }
  bool ProposeBlock(std::shared_ptr<const CBlock> pblock) override {
    ++invocations_ProcessNewBlock;
    return false;
  }
  boost::optional<staking::Coin> GetUTXO(const COutPoint &outpoint) const override {
    ++invocations_GetUTXO;
    return stub_GetUTXO(outpoint);
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
  staking::BlockValidationResult CheckStake(const CBlock &, staking::BlockValidationInfo *) const override {
    return staking::BlockValidationResult();
  }
  uint256 ComputeStakeModifier(const CBlockIndex *, const staking::Coin &) const override { return uint256(); }
  bool IsPieceOfStakeKnown(const COutPoint &) const override { return false; }
  void RememberPieceOfStake(const COutPoint &) override {}
  void ForgetPieceOfStake(const COutPoint &) override {}
};

class CoinsViewMock : public AccessibleCoinsView {

 public:
  Coin default_coin;
  bool default_have_inputs = true;

  mutable std::atomic<std::uint32_t> invocations_AccessCoins{0};
  mutable std::atomic<std::uint32_t> invocations_HaveInputs{0};

  mutable std::function<const Coin &(const COutPoint &)> access_coin = [this](const COutPoint &op) -> const Coin & {
    return default_coin;
  };

  mutable std::function<bool(const CTransaction &)> have_inputs = [this](const CTransaction &tx) -> bool {
    return default_have_inputs;
  };

  const Coin &AccessCoin(const COutPoint &output) const override {
    ++invocations_AccessCoins;
    return access_coin(output);
  }

  bool HaveInputs(const CTransaction &tx) const override {
    ++invocations_HaveInputs;
    return have_inputs(tx);
  }
};

class StateDBMock : public finalization::StateDB {
  using FinalizationState = finalization::FinalizationState;
public:
  mutable std::atomic<std::uint32_t> invocations_Save{0};
  mutable std::atomic<std::uint32_t> invocations_Load{0};
  mutable std::atomic<std::uint32_t> invocations_LoadParticular{0};
  mutable std::atomic<std::uint32_t> invocations_FindLastFinalizedEpoch{0};
  mutable std::atomic<std::uint32_t> invocations_LoadStatesHigherThan{0};

  bool Save(const std::map<const CBlockIndex *, FinalizationState> &states) override {
    ++invocations_Save;
    return false;
  }

  bool Load(const esperanza::FinalizationParams &fin_params,
            const esperanza::AdminParams &admin_params,
            std::map<const CBlockIndex *, FinalizationState> *states) override {
    ++invocations_Load;
    return false;
  }

  bool Load(const CBlockIndex &index,
            const esperanza::FinalizationParams &fin_params,
            const esperanza::AdminParams &admin_params,
            std::map<const CBlockIndex *, FinalizationState> *states) const override {
    ++invocations_LoadParticular;
    return false;
  }

  boost::optional<uint32_t> FindLastFinalizedEpoch(
      const esperanza::FinalizationParams &fin_params,
      const esperanza::AdminParams &admin_params) const override {
    ++invocations_FindLastFinalizedEpoch;
    return boost::none;
  }

  void LoadStatesHigherThan(
      blockchain::Height height,
      const esperanza::FinalizationParams &fin_params,
      const esperanza::AdminParams &admin_params,
      std::map<const CBlockIndex *, FinalizationState> *states) const override {
    ++invocations_LoadStatesHigherThan;
  }
};

class BlockDBMock : public ::BlockDB {
public:
  mutable std::atomic<std::uint32_t> invocations_ReadBlock{0};

  boost::optional<CBlock> ReadBlock(const CBlockIndex &index) override {
    ++invocations_ReadBlock;
    return boost::none;
  }
};

class BlockValidatorMock : public staking::BlockValidator {
  using BlockValidationResult = staking::BlockValidationResult;
  using BlockValidationInfo = staking::BlockValidationInfo;

 public:
  mutable std::atomic<std::uint32_t> invocations_CheckBlock{0};
  mutable std::atomic<std::uint32_t> invocations_CheckBlockHeader{0};
  mutable std::atomic<std::uint32_t> invocations_ContextualCheckBlock{0};
  mutable std::atomic<std::uint32_t> invocations_ContextualCheckBlockHeader{0};

  mutable BlockValidationResult result_CheckBlock;
  mutable BlockValidationResult result_ContextualCheckBlock;
  mutable BlockValidationResult result_CheckBlockHeader;
  mutable BlockValidationResult result_ContextualCheckBlockHeader;

  mutable std::function<BlockValidationResult(const CBlock &, BlockValidationInfo *)> stub_CheckBlock =
      [&](const CBlock &block, BlockValidationInfo *info) {
        return result_CheckBlock;
      };
  mutable std::function<BlockValidationResult(const CBlockHeader &, BlockValidationInfo *)> stub_CheckBlockHeader =
      [&](const CBlockHeader &block_header, BlockValidationInfo *info) {
        return result_CheckBlockHeader;
      };
  mutable std::function<BlockValidationResult(const CBlock &, const CBlockIndex &, blockchain::Time, BlockValidationInfo *)> stub_ContextualCheckBlock =
      [&](const CBlock &block, const CBlockIndex &block_index, blockchain::Time adjusted_time, BlockValidationInfo *info) {
        return result_ContextualCheckBlock;
      };
  mutable std::function<BlockValidationResult(const CBlockHeader &, const CBlockIndex &, blockchain::Time, BlockValidationInfo *)> stub_ContextualCheckBlockHeader =
      [&](const CBlockHeader &block_header, const CBlockIndex &block_index, blockchain::Time time, BlockValidationInfo *info) {
        return result_ContextualCheckBlockHeader;
      };

  BlockValidationResult CheckBlock(const CBlock &block, BlockValidationInfo *info) const override {
    ++invocations_CheckBlock;
    return stub_CheckBlock(block, info);
  }
  BlockValidationResult ContextualCheckBlock(const CBlock &block, const CBlockIndex &block_index, blockchain::Time adjusted_time, BlockValidationInfo *info) const override {
    ++invocations_ContextualCheckBlock;
    return stub_ContextualCheckBlock(block, block_index, adjusted_time, info);
  }
  BlockValidationResult CheckBlockHeader(const CBlockHeader &block_header, BlockValidationInfo *info) const override {
    ++invocations_CheckBlockHeader;
    return stub_CheckBlockHeader(block_header, info);
  }
  BlockValidationResult ContextualCheckBlockHeader(const CBlockHeader &block_header, const CBlockIndex &block_index, blockchain::Time time, BlockValidationInfo *info) const override {
    ++invocations_ContextualCheckBlockHeader;
    return stub_ContextualCheckBlockHeader(block_header, block_index, time, info);
  }
};

}  // namespace mocks

#endif  //UNIT_E_TEST_UNITE_MOCKS_H
