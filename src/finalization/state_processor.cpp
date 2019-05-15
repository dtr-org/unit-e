// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#include <finalization/state_processor.h>

#include <esperanza/finalizationstate.h>
#include <finalization/state_repository.h>
#include <snapshot/creator.h>
#include <staking/active_chain.h>

namespace finalization {
namespace {

class ProcessorImpl final : public StateProcessor {
 public:
  explicit ProcessorImpl(Dependency<finalization::Params> finalization_params,
                         Dependency<finalization::StateRepository> repo,
                         Dependency<staking::ActiveChain> active_chain)
      : m_finalization_params(finalization_params),
        m_repo(repo),
        m_active_chain(active_chain) {}

  bool ProcessNewCommits(const CBlockIndex &block_index, const std::vector<CTransactionRef> &txes) override;
  bool ProcessNewTipCandidate(const CBlockIndex &block_index, const CBlock &block) override;
  bool ProcessNewTip(const CBlockIndex &block_index, const CBlock &block) override;

 private:
  bool ProcessNewTipWorker(const CBlockIndex &block_index, const CBlock &block);
  bool FinalizationHappened(const CBlockIndex &block_index);

  Dependency<finalization::Params> m_finalization_params;
  Dependency<finalization::StateRepository> m_repo;
  Dependency<staking::ActiveChain> m_active_chain;
};

bool ProcessorImpl::ProcessNewTipWorker(const CBlockIndex &block_index, const CBlock &block) {
  AssertLockHeld(m_repo->GetLock());
  const auto state = m_repo->FindOrCreate(block_index, FinalizationState::FROM_COMMITS);
  if (state == nullptr) {
    LogPrint(BCLog::FINALIZATION, "Cannot find or create finalization state for %s\n",
             block_index.GetBlockHash().GetHex());
    return false;
  }

  switch (state->GetInitStatus()) {
    case FinalizationState::NEW: {
      state->ProcessNewTip(block_index, block);
      break;
    }

    case FinalizationState::FROM_COMMITS: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s heigh=%d has been processed from commits, confirming...\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      assert(block_index.pprev != nullptr);  // we don't process commits of genesis block
      const auto ancestor_state = m_repo->Find(*block_index.pprev);
      assert(ancestor_state != nullptr);
      FinalizationState new_state(*ancestor_state);
      new_state.ProcessNewTip(block_index, block);
      if (!m_repo->Confirm(block_index, std::move(new_state), nullptr)) {
        // UNIT-E TODO: DoS commits sender.
        LogPrint(BCLog::FINALIZATION, "WARN: After processing the block_hash=%s height=%d, its finalization state differs from one given from commits. Overwrite it anyway.\n",
                 block_index.GetBlockHash().GetHex(), block_index.nHeight);
      } else {
        LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d confirmed\n",
                 block_index.GetBlockHash().GetHex(), block_index.nHeight);
      }
      break;
    }

    case FinalizationState::COMPLETED: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      break;
    }
  }

  return true;
}

bool ProcessorImpl::FinalizationHappened(const CBlockIndex &block_index) {
  AssertLockHeld(m_repo->GetLock());

  if (block_index.pprev == nullptr) {
    return false;
  }

  const auto *prev_state = m_repo->Find(*block_index.pprev);
  const auto *new_state = m_repo->Find(block_index);
  if (prev_state == nullptr || new_state == nullptr) {
    return false;
  }

  const uint32_t prev_fin_epoch = prev_state->GetLastFinalizedEpoch();
  const uint32_t new_fin_epoch = new_state->GetLastFinalizedEpoch();
  if (prev_fin_epoch == new_fin_epoch) {
    return false;
  }

  assert(new_fin_epoch > prev_fin_epoch);
  return true;
}

bool ProcessorImpl::ProcessNewTip(const CBlockIndex &block_index, const CBlock &block) {
  LOCK(m_repo->GetLock());

  LogPrint(BCLog::FINALIZATION, "Process tip block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);

  if (!ProcessNewTipWorker(block_index, block)) {
    return false;
  }

  const uint32_t epoch_length = m_finalization_params->epoch_length;
  if (block_index.nHeight > 0 && !m_repo->Restoring() &&
      (block_index.nHeight + 1) % epoch_length == 0) {
    // Generate the snapshot for the block which is one block behind the last one.
    // The last epoch block will contain the snapshot hash pointing to this snapshot.
    snapshot::Creator::GenerateOrSkip(m_repo->GetTipState()->GetCurrentEpoch());
  }

  if (FinalizationHappened(block_index)) {
    esperanza::FinalizationState *state = m_repo->Find(block_index);
    assert(state);

    // We cannot make forks before this point as they can revert finalization.
    const uint32_t checkpoint_height = state->GetEpochCheckpointHeight(state->GetLastFinalizedEpoch());
    m_repo->TrimUntilHeight(checkpoint_height);

    snapshot::Creator::FinalizeSnapshots(m_active_chain->AtHeight(checkpoint_height));
  }
  return true;
}

bool ProcessorImpl::ProcessNewTipCandidate(const CBlockIndex &block_index, const CBlock &block) {
  LogPrint(BCLog::FINALIZATION, "Process candidate tip block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);
  LOCK(m_repo->GetLock());
  return ProcessNewTipWorker(block_index, block);
}

bool ProcessorImpl::ProcessNewCommits(const CBlockIndex &block_index, const std::vector<CTransactionRef> &txes) {
  LogPrint(BCLog::FINALIZATION, "Process commits block_hash=%s height=%d\n",
           block_index.GetBlockHash().GetHex(), block_index.nHeight);

  LOCK(m_repo->GetLock());

  const auto state = m_repo->FindOrCreate(block_index, FinalizationState::FROM_COMMITS);
  if (state == nullptr) {
    LogPrint(BCLog::FINALIZATION, "Cannot find or create finalization state for %s\n",
             block_index.GetBlockHash().GetHex());
    return false;
  }

  switch (state->GetInitStatus()) {
    case esperanza::FinalizationState::NEW: {
      state->ProcessNewCommits(block_index, txes);
      break;
    }

    case esperanza::FinalizationState::FROM_COMMITS: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed from commits\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      break;
    }

    case esperanza::FinalizationState::COMPLETED: {
      LogPrint(BCLog::FINALIZATION, "State for block_hash=%s height=%d has been already processed\n",
               block_index.GetBlockHash().GetHex(), block_index.nHeight);
      break;
    }
  }

  return true;
}

}  // namespace

std::unique_ptr<StateProcessor> StateProcessor::New(
    Dependency<finalization::Params> finalization_params,
    Dependency<finalization::StateRepository> repo,
    Dependency<staking::ActiveChain> active_chain) {
  return MakeUnique<ProcessorImpl>(finalization_params, repo, active_chain);
}

}  // namespace finalization
