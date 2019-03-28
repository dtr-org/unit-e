// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_TEST_UNITE_MOCKS_H
#define UNIT_E_TEST_UNITE_MOCKS_H

#include <blockdb.h>
#include <coins.h>
#include <esperanza/finalizationstate.h>
#include <finalization/state_db.h>
#include <proposer/block_builder.h>
#include <proposer/proposer_logic.h>
#include <staking/active_chain.h>
#include <staking/block_index_map.h>
#include <staking/block_validator.h>
#include <staking/network.h>
#include <staking/stake_validator.h>
#include <staking/transactionpicker.h>
#include <util.h>

#include <test/util/mocks.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <type_traits>

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
    std::string error;
    ParseParameters(static_cast<int>(i), argv, error);
    delete[] argv;
  }
  bool IsArgKnown(const std::string &) const override { return true; }
};

class NetworkMock : public staking::Network, public Mock {
 public:
  MethodMock<decltype(&staking::Network::GetTime)> mock_GetTime{this};
  MethodMock<decltype(&staking::Network::GetNodeCount)> mock_GetNodeCount{this};
  MethodMock<decltype(&staking::Network::GetInboundNodeCount)> mock_GetInboundNodeCount{this};
  MethodMock<decltype(&staking::Network::GetOutboundNodeCount)> mock_GetOutboundNodeCount{this};

  int64_t GetTime() const override {
    return mock_GetTime();
  }
  std::size_t GetNodeCount() const override {
    return mock_GetNodeCount();
  }
  std::size_t GetInboundNodeCount() const override {
    return mock_GetInboundNodeCount();
  }
  std::size_t GetOutboundNodeCount() const override {
    return mock_GetOutboundNodeCount();
  }
};

class BlockIndexMapMock : public staking::BlockIndexMap, public Mock {
 public:
  MethodMock<decltype(&staking::BlockIndexMap::GetLock)> mock_GetLock{this};
  MethodMock<decltype(&staking::BlockIndexMap::Lookup)> mock_Lookup{this, nullptr};
  MethodMock<decltype(&staking::BlockIndexMap::ForEach)> mock_ForEach{this};

  CCriticalSection &GetLock() const override {
    return mock_GetLock();
  }
  CBlockIndex *Lookup(const uint256 &block_hash) const override {
    return mock_Lookup(block_hash);
  }
  void ForEach(std::function<bool(const uint256 &, const CBlockIndex &)> &&f) const override {
    mock_ForEach.forward(std::move(f));
  }
};

class BlockIndexMapFake : public BlockIndexMapMock {
 public:
  bool reverse = false;

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
  ~BlockIndexMapFake() override {
    for (auto &i : indexes) {
      delete i.second;
    }
  }

 private:
  std::map<uint256, CBlockIndex *> indexes;
};

class ActiveChainMock : public staking::ActiveChain, public Mock {
 public:
  MethodMock<decltype(&staking::ActiveChain::GetLock)> mock_GetLock{this};
  MethodMock<decltype(&staking::ActiveChain::GetSize)> mock_GetSize{this, 0};
  MethodMock<decltype(&staking::ActiveChain::GetHeight)> mock_GetHeight{this, 1};
  MethodMock<decltype(&staking::ActiveChain::GetTip)> mock_GetTip{this, nullptr};
  MethodMock<decltype(&staking::ActiveChain::GetGenesis)> mock_GetGenesis{this, nullptr};
  MethodMock<decltype(&staking::ActiveChain::Contains)> mock_Contains{this, false};
  MethodMock<decltype(&staking::ActiveChain::FindForkOrigin)> mock_FindForkOrigin{this};
  MethodMock<decltype(&staking::ActiveChain::GetNext)> mock_GetNext{this};
  MethodMock<decltype(&staking::ActiveChain::AtDepth)> mock_AtDepth{this};
  MethodMock<decltype(&staking::ActiveChain::AtHeight)> mock_AtHeight{this};
  MethodMock<decltype(&staking::ActiveChain::GetDepth)> mock_GetDepth{this};
  MethodMock<decltype(&staking::ActiveChain::GetBlockIndex)> mock_GetBlockIndex{this};
  MethodMock<decltype(&staking::ActiveChain::ComputeSnapshotHash)> mock_ComputeSnapshotHash{this};
  MethodMock<decltype(&staking::ActiveChain::ProposeBlock)> mock_ProposeBlock{this};
  MethodMock<decltype(&staking::ActiveChain::GetUTXO)> mock_GetUTXO{this};
  MethodMock<decltype(&staking::ActiveChain::GetInitialBlockDownloadStatus)> mock_GetInitialBlockDownloadStatus{this, SyncStatus::SYNCED};

  CCriticalSection &GetLock() const override {
    return mock_GetLock();
  }
  blockchain::Height GetSize() const override {
    return mock_GetSize();
  }
  blockchain::Height GetHeight() const override {
    return mock_GetHeight();
  }
  const CBlockIndex *GetTip() const override {
    return mock_GetTip();
  }
  const CBlockIndex *GetGenesis() const override {
    return mock_GetGenesis();
  }
  bool Contains(const CBlockIndex &block_index) const override {
    return mock_Contains(block_index);
  }
  const CBlockIndex *FindForkOrigin(const CBlockIndex &block_index) const override {
    return mock_FindForkOrigin(block_index);
  }
  const CBlockIndex *GetNext(const CBlockIndex &block_index) const override {
    return mock_GetNext(block_index);
  }
  const CBlockIndex *AtDepth(blockchain::Depth depth) const override {
    return mock_AtDepth(depth);
  }
  const CBlockIndex *AtHeight(blockchain::Height height) const override {
    return mock_AtHeight(height);
  }
  blockchain::Depth GetDepth(const blockchain::Height height) const override {
    return mock_GetDepth(height);
  }
  const CBlockIndex *GetBlockIndex(const uint256 &hash) const override {
    return mock_GetBlockIndex(hash);
  }
  const uint256 ComputeSnapshotHash() const override {
    return mock_ComputeSnapshotHash();
  }
  bool ProposeBlock(std::shared_ptr<const CBlock> pblock) override {
    return mock_ProposeBlock(pblock);
  }
  boost::optional<staking::Coin> GetUTXO(const COutPoint &outpoint) const override {
    return mock_GetUTXO(outpoint);
  }
  ::SyncStatus GetInitialBlockDownloadStatus() const override {
    return mock_GetInitialBlockDownloadStatus();
  }
};

class ActiveChainFake : public ActiveChainMock {
 public:
  ActiveChainFake() {
    mock_GetSize.SetStub([this]() { return GetHeight() + 1; });
    mock_Contains.SetStub([this](const CBlockIndex &ix) { return AtHeight(ix.nHeight) == &ix; });
    mock_FindForkOrigin.SetStub([this](const CBlockIndex &block_index) {
      const CBlockIndex *walk = &block_index;
      while (walk != nullptr && AtHeight(walk->nHeight) != walk) {
        walk = walk->pprev;
      }
      return walk;
    });
    mock_GetNext.SetStub([this](const CBlockIndex &block_index) -> const CBlockIndex * {
      if (AtHeight(block_index.nHeight) == &block_index) {
        return AtHeight(block_index.nHeight + 1);
      }
      return nullptr;
    });
    mock_GetDepth.SetStub([this](const blockchain::Height height) {
      return GetHeight() - height + 1;
    });
  }
};

class StakeValidatorMock : public staking::StakeValidator, public Mock {
  mutable CCriticalSection lock;

 public:
  MethodMock<decltype(&staking::StakeValidator::GetLock)> mock_GetLock{this};
  MethodMock<decltype(&staking::StakeValidator::CheckKernel)> mock_CheckKernel{this, false};
  MethodMock<decltype(&staking::StakeValidator::ComputeKernelHash)> mock_ComputeKernelHash{this, uint256::zero};
  MethodMock<decltype(&staking::StakeValidator::ComputeStakeModifier)> mock_ComputeStakeModifier{this, uint256::zero};
  MethodMock<decltype(&staking::StakeValidator::IsPieceOfStakeKnown)> mock_IsPieceOfStakeKnown{this, false};
  MethodMock<decltype(&staking::StakeValidator::RememberPieceOfStake)> mock_RememberPieceOfStake{this};
  MethodMock<decltype(&staking::StakeValidator::ForgetPieceOfStake)> mock_ForgetPieceOfStake{this};
  MethodMock<decltype(&staking::StakeValidator::IsStakeMature)> mock_IsStakeMature{this, true};

  CCriticalSection &GetLock() override {
    return mock_GetLock();
  }
  bool CheckKernel(const CAmount amount, const uint256 &kernel, blockchain::Difficulty difficulty) const override {
    return mock_CheckKernel(amount, kernel, difficulty);
  }
  uint256 ComputeKernelHash(const CBlockIndex *blockindex, const staking::Coin &coin, blockchain::Time time) const override {
    return mock_ComputeKernelHash(blockindex, coin, time);
  }
  uint256 ComputeStakeModifier(const CBlockIndex *blockindex, const staking::Coin &coin) const override {
    return mock_ComputeStakeModifier(blockindex, coin);
  }
  bool IsPieceOfStakeKnown(const COutPoint &outpoint) const override {
    return mock_IsPieceOfStakeKnown(outpoint);
  }
  void RememberPieceOfStake(const COutPoint &outpoint) override {
    return mock_RememberPieceOfStake(outpoint);
  }
  void ForgetPieceOfStake(const COutPoint &outpoint) override {
    return mock_ForgetPieceOfStake(outpoint);
  }
  bool IsStakeMature(const blockchain::Height height) const override {
    return mock_IsStakeMature(height);
  };

 protected:
  blockchain::UTXOView &GetUTXOView() const override {
    static mocks::ActiveChainMock active_chain_mock;
    return active_chain_mock;
  }
  staking::BlockValidationResult CheckStake(
      const CBlock &block,
      const blockchain::UTXOView &utxo_view,
      CheckStakeFlags::Type flags,
      staking::BlockValidationInfo *info) const override {
    return staking::BlockValidationResult();
  }
};

class CoinsViewMock : public AccessibleCoinsView, public Mock {

 public:
  MethodMock<decltype(&AccessibleCoinsView::AccessCoin)> mock_AccessCoin{this, {}};
  MethodMock<decltype(&AccessibleCoinsView::HaveInputs)> mock_HaveInputs{this, true};

  const Coin &AccessCoin(const COutPoint &outpoint) const override {
    return mock_AccessCoin(outpoint);
  }
  bool HaveInputs(const CTransaction &tx) const override {
    return mock_HaveInputs(tx);
  }
};

class StateDBMock : public finalization::StateDB, public Mock {
  using FinalizationState = esperanza::FinalizationState;

 public:
  MethodMock<decltype(&finalization::StateDB::Save)> mock_Save{this, false};
  MethodMock<bool (finalization::StateDB::*)(std::map<const CBlockIndex *, FinalizationState> *)> mock_Load{this, false};
  MethodMock<bool (finalization::StateDB::*)(const CBlockIndex &, std::map<const CBlockIndex *, FinalizationState> *)> mock_LoadParticular{this, false};
  MethodMock<decltype(&finalization::StateDB::FindLastFinalizedEpoch)> mock_FindLastFinalizedEpoch{this, boost::none};
  MethodMock<decltype(&finalization::StateDB::LoadStatesHigherThan)> mock_LoadStatesHigherThan{this};

  bool Save(const std::map<const CBlockIndex *, FinalizationState> &states) override {
    return mock_Save(states);
  }
  bool Load(std::map<const CBlockIndex *, FinalizationState> *states) override {
    return mock_Load(states);
  }
  bool Load(const CBlockIndex &index,
            std::map<const CBlockIndex *, FinalizationState> *states) const override {
    return mock_LoadParticular(index, states);
  }
  boost::optional<uint32_t> FindLastFinalizedEpoch() const override {
    return mock_FindLastFinalizedEpoch();
  }
  void LoadStatesHigherThan(
      blockchain::Height height,
      std::map<const CBlockIndex *, FinalizationState> *states) const override {
    mock_LoadStatesHigherThan(height, states);
  }
};

class BlockDBMock : public BlockDB, public Mock {
 public:
  MethodMock<decltype(&BlockDB::ReadBlock)> mock_ReadBlock{this, boost::none};

  boost::optional<CBlock> ReadBlock(const CBlockIndex &index) override {
    return mock_ReadBlock(index);
  }
};

class BlockValidatorMock : public staking::BlockValidator, public Mock {
  using BlockValidationResult = staking::BlockValidationResult;
  using BlockValidationInfo = staking::BlockValidationInfo;

 public:
  MethodMock<decltype(&staking::BlockValidator::CheckBlock)> mock_CheckBlock{this};
  MethodMock<decltype(&staking::BlockValidator::CheckBlockHeader)> mock_CheckBlockHeader{this};
  MethodMock<decltype(&staking::BlockValidator::ContextualCheckBlock)> mock_ContextualCheckBlock{this};
  MethodMock<decltype(&staking::BlockValidator::ContextualCheckBlockHeader)> mock_ContextualCheckBlockHeader{this};
  MethodMock<decltype(&staking::BlockValidator::CheckTransaction)> mock_CheckTransaction{this};
  MethodMock<decltype(&staking::BlockValidator::CheckCoinbaseTransaction)> mock_CheckCoinbaseTransaction{this};

  BlockValidationResult CheckBlock(const CBlock &block, BlockValidationInfo *info) const override {
    return mock_CheckBlock(block, info);
  }
  BlockValidationResult ContextualCheckBlock(const CBlock &block, const CBlockIndex &block_index, blockchain::Time adjusted_time, BlockValidationInfo *info) const override {
    return mock_ContextualCheckBlock(block, block_index, adjusted_time, info);
  }
  BlockValidationResult CheckBlockHeader(const CBlockHeader &block_header, BlockValidationInfo *info) const override {
    return mock_CheckBlockHeader(block_header, info);
  }
  BlockValidationResult ContextualCheckBlockHeader(const CBlockHeader &block_header, const CBlockIndex &block_index, blockchain::Time time, BlockValidationInfo *info) const override {
    return mock_ContextualCheckBlockHeader(block_header, block_index, time, info);
  }
  BlockValidationResult CheckTransaction(const CTransaction &tx) const override {
    return mock_CheckTransaction(tx);
  }
  BlockValidationResult CheckCoinbaseTransaction(const CBlock &block, const CTransaction &coinbase_tx) const override {
    return mock_CheckCoinbaseTransaction(block, coinbase_tx);
  }
};

class ProposerLogicMock : public proposer::Logic, public Mock {
 public:
  MethodMock<decltype(&proposer::Logic::TryPropose)> mock_TryPropose{this, boost::none};

  boost::optional<proposer::EligibleCoin> TryPropose(const staking::CoinSet &coin_set) override {
    return mock_TryPropose(coin_set);
  }
};

class TransactionPickerMock : public staking::TransactionPicker, public Mock {

 public:
  MethodMock<decltype(&staking::TransactionPicker::PickTransactions)> mock_PickTransactions{this, {"", {}, {}}};

  PickTransactionsResult PickTransactions(const PickTransactionsParameters &parameters) override {
    return mock_PickTransactions(parameters);
  }
};

class BlockBuilderMock : public proposer::BlockBuilder, public Mock {
 public:
  MethodMock<decltype(&proposer::BlockBuilder::BuildCoinbaseTransaction)> mock_BuildCoinbaseTransaction{this};
  MethodMock<decltype(&proposer::BlockBuilder::BuildBlock)> mock_BuildBlock{this};

  const CTransactionRef BuildCoinbaseTransaction(
      const CBlockIndex &prev_block,
      const uint256 &snapshot_hash,
      const proposer::EligibleCoin &eligible_coin,
      const staking::CoinSet &coins,
      const CAmount fees,
      const boost::optional<CScript> &coinbase_script,
      staking::StakingWallet &wallet) const override {
    return mock_BuildCoinbaseTransaction(prev_block, snapshot_hash, eligible_coin, coins, fees, coinbase_script, wallet);
  }
  std::shared_ptr<const CBlock> BuildBlock(
      const CBlockIndex &index,
      const uint256 &snapshot_hash,
      const proposer::EligibleCoin &stake_coin,
      const staking::CoinSet &coins,
      const std::vector<CTransactionRef> &txs,
      const CAmount fees,
      const boost::optional<CScript> &coinbase_script,
      staking::StakingWallet &wallet) const override {
    return mock_BuildBlock(index, snapshot_hash, stake_coin, coins, txs, fees, coinbase_script, wallet);
  }
};

}  // namespace mocks

#endif  //UNIT_E_TEST_UNITE_MOCKS_H
