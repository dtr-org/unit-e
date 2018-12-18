// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/creator.h>

#include <snapshot/indexer.h>
#include <snapshot/p2p_processing.h>
#include <snapshot/snapshot_index.h>
#include <snapshot/state.h>
#include <sync.h>
#include <util.h>
#include <validation.h>

#include <atomic>
#include <queue>
#include <thread>

namespace snapshot {

struct SnapshotJob {
  // create snapshot
  std::unique_ptr<Creator> creator = nullptr;

  // finalize snapshots
  const CBlockIndex *block_index = nullptr;

  explicit SnapshotJob(std::unique_ptr<Creator> _creator)
      : creator(std::move(_creator)) {}

  explicit SnapshotJob(const CBlockIndex *_block_index)
      : block_index(_block_index) {}
};

uint16_t g_create_snapshot_per_epoch = 0;
std::thread g_creator_thread;
std::mutex mutex;
std::condition_variable cv;
std::queue<std::unique_ptr<SnapshotJob>> jobs;
std::atomic_bool interrupt(false);

void ProcessCreatorQueue() {
  while (!interrupt) {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [] { return !jobs.empty() || interrupt; });

    if (interrupt) {
      continue;
    }

    std::unique_ptr<SnapshotJob> job = std::move(jobs.front());
    jobs.pop();
    lock.unlock();

    if (job->creator) {
      CreationInfo info = job->creator->Create();
      if (info.status != +Status::OK) {
        LogPrint(BCLog::SNAPSHOT, "%s: can't create snapshot %s\n",
                 __func__, info.status);
      }
    }

    if (job->block_index) {
      FinalizeSnapshots(job->block_index);
    }

    SaveSnapshotIndex();
  }

  LogPrint(BCLog::SNAPSHOT, "%s: interrupted\n", __func__);
}

void Creator::Init(const Params &params) {
  g_create_snapshot_per_epoch = params.create_snapshot_per_epoch;
  g_creator_thread = std::thread(ProcessCreatorQueue);
}

void Creator::Deinit() {
  LogPrint(BCLog::SNAPSHOT, "stopping snapshot creation thread...\n");
  interrupt = true;
  cv.notify_one();
  g_creator_thread.join();

  // clean unprocessed jobs
  while (!jobs.empty()) {
    jobs.pop();
  }
}

Creator::Creator(CCoinsViewDB *view) : m_iter(view) {}

void Creator::GenerateOrSkip(uint32_t currentEpoch) {
  if (g_create_snapshot_per_epoch <= 0) {
    return;
  }

  // disable if node is syncing with the chain using either full or fast sync
  if (IsInitialBlockDownload() ||
      (IsISDEnabled() && IsInitialSnapshotDownload())) {
    return;
  }

  if (currentEpoch > 0 && (currentEpoch + 1) % g_create_snapshot_per_epoch != 0) {
    return;
  }

  // ensure that pcoinsTip flushes its data to disk as creator
  // uses disk data to create the snapshot
  FlushStateToDisk();

  std::unique_ptr<SnapshotJob> job(new SnapshotJob(MakeUnique<Creator>(pcoinsdbview.get())));
  std::lock_guard<std::mutex> lock(mutex);
  jobs.push(std::move(job));
  cv.notify_one();
}

void Creator::FinalizeSnapshots(const CBlockIndex *blockIndex) {
  std::unique_ptr<SnapshotJob> job(new SnapshotJob(blockIndex));
  std::lock_guard<std::mutex> lock(mutex);
  jobs.push(std::move(job));
  cv.notify_one();
}

CCriticalSection cs_snapshotCreation;

CreationInfo Creator::Create() {
  LOCK(cs_snapshotCreation);

  CBlockIndex *blockIndex = mapBlockIndex.at(m_iter.GetBestBlock());
  uint256 snapshotHash = m_iter.GetSnapshotHash().GetHash(
      blockIndex->bnStakeModifier);
  std::vector<uint256> toRemove = AddSnapshotHash(snapshotHash, blockIndex);

  LogPrint(BCLog::SNAPSHOT, "start creating snapshot block_hash=%s snapshot_hash=%s\n",
           m_iter.GetBestBlock().GetHex(), snapshotHash.GetHex());

  CreationInfo info;
  Indexer indexer(snapshotHash, blockIndex->GetBlockHash(),
                  blockIndex->bnStakeModifier, m_step, m_stepsPerFile);

  while (m_iter.Valid()) {
    boost::this_thread::interruption_point();

    UTXOSubset subset = m_iter.GetUTXOSubset();
    info.total_outputs += subset.outputs.size();

    if (!indexer.WriteUTXOSubset(subset)) {
      info.status = Status::WRITE_ERROR;
      return info;
    }

    if (indexer.GetMeta().total_utxo_subsets == m_maxUTXOSubsets) {
      break;
    }

    m_iter.Next();
  }

  if (!indexer.Flush()) {
    info.status = Status::WRITE_ERROR;
    return info;
  }

  if (indexer.GetMeta().snapshot_hash.IsNull()) {
    info.status = Status::CALC_SNAPSHOT_HASH_ERROR;
    return info;
  }
  info.indexer_meta = indexer.GetMeta();

  LogPrint(BCLog::SNAPSHOT, "snapshot_hash=%s is created\n", snapshotHash.GetHex());

  for (const auto &hash : toRemove) {
    if (Indexer::Delete(hash)) {
      ConfirmRemoved(hash);
      LogPrint(BCLog::SNAPSHOT, "snapshot_hash=%s is deleted\n", hash.GetHex());
    }
  }

  return info;
}

bool IsRecurrentCreation() {
  return g_create_snapshot_per_epoch > 0;
}

}  // namespace snapshot
