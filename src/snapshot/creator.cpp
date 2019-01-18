// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/creator.h>

#include <arith_uint256.h>
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

namespace {
uint16_t g_create_snapshot_per_epoch = 0;
std::thread g_creator_thread;
std::mutex mutex;
std::condition_variable cv;
std::queue<std::unique_ptr<SnapshotJob>> jobs;
std::atomic_bool interrupt(false);
}  // namespace

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

void Creator::GenerateOrSkip(const uint32_t current_epoch) {
  if (g_create_snapshot_per_epoch <= 0) {
    return;
  }

  // disable if node is syncing with the chain using either full or fast sync
  if (IsInitialBlockDownload() ||
      (IsISDEnabled() && IsInitialSnapshotDownload())) {
    return;
  }

  if (current_epoch > 0 && (current_epoch + 1) % g_create_snapshot_per_epoch != 0) {
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

void Creator::FinalizeSnapshots(const CBlockIndex *block_index) {
  std::unique_ptr<SnapshotJob> job(new SnapshotJob(block_index));
  std::lock_guard<std::mutex> lock(mutex);
  jobs.push(std::move(job));
  cv.notify_one();
}

CCriticalSection cs_snapshot_creation;

CreationInfo Creator::Create() {
  LOCK(cs_snapshot_creation);

  CreationInfo info;

  CBlockIndex *block_index = mapBlockIndex.at(m_iter.GetBestBlock());

  SnapshotHeader snapshot_header;
  snapshot_header.block_hash = block_index->GetBlockHash();
  snapshot_header.stake_modifier = block_index->stake_modifier;
  snapshot_header.chain_work = ArithToUint256(block_index->nChainWork);
  snapshot_header.snapshot_hash = m_iter.GetSnapshotHash().GetHash(
      snapshot_header.stake_modifier, snapshot_header.chain_work);

  LogPrint(BCLog::SNAPSHOT, "start creating snapshot block_hash=%s snapshot_hash=%s\n",
           snapshot_header.block_hash.GetHex(), snapshot_header.snapshot_hash.GetHex());

  std::vector<uint256> to_remove = AddSnapshotHash(snapshot_header.snapshot_hash, block_index);

  Indexer indexer(snapshot_header, m_step, m_steps_per_file);

  while (m_iter.Valid()) {
    boost::this_thread::interruption_point();

    UTXOSubset subset = m_iter.GetUTXOSubset();
    info.total_outputs += subset.outputs.size();

    if (!indexer.WriteUTXOSubset(subset)) {
      info.status = Status::WRITE_ERROR;
      return info;
    }

    if (indexer.GetSnapshotHeader().total_utxo_subsets == m_max_utxo_subsets) {
      break;
    }

    m_iter.Next();
  }

  if (!indexer.Flush()) {
    info.status = Status::WRITE_ERROR;
    return info;
  }

  info.snapshot_header = indexer.GetSnapshotHeader();

  LogPrint(BCLog::SNAPSHOT, "snapshot_hash=%s is created\n",
           info.snapshot_header.snapshot_hash.GetHex());

  for (const auto &hash : to_remove) {
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
