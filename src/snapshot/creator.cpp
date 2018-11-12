// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/creator.h>

#include <snapshot/indexer.h>
#include <snapshot/p2p_processing.h>
#include <snapshot/state.h>
#include <sync.h>
#include <util.h>
#include <validation.h>

#include <atomic>
#include <queue>
#include <thread>

namespace snapshot {

int64_t createSnapshotPerEpoch = 0;

std::thread creatorThread;
std::mutex mutex;
std::condition_variable cv;
std::queue<std::unique_ptr<Creator>> jobs;
std::atomic_bool interrupt(false);

void ProcessCreatorQueue() {
  while (!interrupt) {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [] { return !jobs.empty() || interrupt; });

    if (interrupt) {
      continue;
    }

    std::unique_ptr<Creator> creator = std::move(jobs.front());
    jobs.pop();
    lock.unlock();

    CreationInfo info = creator->Create();
    if (info.m_status != +Status::OK) {
      LogPrint(BCLog::SNAPSHOT, "%s: ERROR: can't create snapshot %s\n", __func__, info.m_status);
    }
  }

  LogPrint(BCLog::SNAPSHOT, "%s: interrupted\n", __func__);
}

void Creator::Init(const Params &params) {
  createSnapshotPerEpoch = params.createSnapshotPerEpoch;
  creatorThread = std::thread(ProcessCreatorQueue);
}

void Creator::Deinit() {
  LogPrint(BCLog::SNAPSHOT, "stopping snapshot creation thread...\n");
  interrupt = true;
  cv.notify_one();
  creatorThread.join();

  // clean unprocessed jobs
  while (!jobs.empty()) {
    jobs.pop();
  }
}

Creator::Creator(CCoinsViewDB *view) : m_view(view), m_iter(view) {}

void Creator::GenerateOrSkip(uint32_t currentEpoch) {
  if (createSnapshotPerEpoch <= 0) {
    return;
  }

  // disable if node is syncing with the chain using either full or fast sync
  if (IsInitialBlockDownload() ||
      (IsISDEnabled() && IsInitialSnapshotDownload())) {
    return;
  }

  if ((currentEpoch + 1) % createSnapshotPerEpoch != 0) {
    return;
  }

  // ensure that pcoinsTip flushes its data to disk as creator
  // uses disk data to create the snapshot
  FlushStateToDisk();

  auto creator = MakeUnique<Creator>(pcoinsdbview.get());
  std::lock_guard<std::mutex> lock(mutex);
  jobs.push(std::move(creator));
  cv.notify_one();
}

CCriticalSection cs_snapshotCreation;

CreationInfo Creator::Create() {
  LOCK(cs_snapshotCreation);

  uint256 stakeModifier = mapBlockIndex.at(m_iter.GetBestBlock())->bnStakeModifier;
  uint256 snapshotHash = m_iter.GetSnapshotHash().GetHash(stakeModifier);
  LogPrint(BCLog::SNAPSHOT, "start creating snapshot block_hash=%s snapshot_hash=%s\n",
           m_iter.GetBestBlock().ToString(), snapshotHash.ToString());

  CreationInfo info;

  uint32_t snapshotId = 0;
  if (!m_view->ReserveSnapshotId(snapshotId)) {
    info.m_status = Status::RESERVE_SNAPSHOT_ID_ERROR;
    return info;
  }
  LogPrint(BCLog::SNAPSHOT, "reserve new snapshot_id=%i for snapshot_hash=%s\n",
           snapshotId, snapshotHash.ToString());

  Indexer indexer(snapshotId, snapshotHash, m_iter.GetBestBlock(), stakeModifier,
                  m_step, m_stepsPerFile);

  while (m_iter.Valid()) {
    boost::this_thread::interruption_point();

    UTXOSubset subset = m_iter.GetUTXOSubset();
    info.m_totalOutputs += subset.m_outputs.size();

    if (!indexer.WriteUTXOSubset(subset)) {
      info.m_status = Status::WRITE_ERROR;
      return info;
    }

    if (indexer.GetMeta().m_totalUTXOSubsets == m_maxUTXOSubsets) {
      break;
    }

    m_iter.Next();
  }

  if (!indexer.Flush()) {
    info.m_status = Status::WRITE_ERROR;
    return info;
  }

  if (indexer.GetMeta().m_snapshotHash.IsNull()) {
    info.m_status = Status::CALC_SNAPSHOT_HASH_ERROR;
    return info;
  }
  info.m_indexerMeta = indexer.GetMeta();

  if (!m_view->SetSnapshotId(snapshotId)) {
    info.m_status = Status::SET_SNAPSHOT_ID_ERROR;
    return info;
  }
  LogPrint(BCLog::SNAPSHOT, "snapshot_id=%i is created\n", snapshotId);

  std::vector<uint32_t> ids = m_view->GetSnapshotIds();
  uint64_t keepSnapshots = 5;
  if (ids.size() > keepSnapshots) {
    std::vector<uint32_t> newIds;
    newIds.reserve(keepSnapshots);

    for (uint64_t i = 0; i < ids.size() - keepSnapshots; ++i) {
      if (Indexer::Delete(ids.at(i))) {
        LogPrint(BCLog::SNAPSHOT, "snapshot_id=%i is deleted\n", ids.at(i));
      } else {
        // collect IDs that can't be deleted,
        // will be re-tried during next snapshot creation
        newIds.emplace_back(ids.at(i));
      }
    }
    newIds.insert(newIds.end(), ids.end() - keepSnapshots, ids.end());

    if (!m_view->SetSnapshotIds(newIds)) {
      info.m_status = Status::SET_ALL_SNAPSHOTS_ERROR;
      return info;
    }
  }

  return info;
}

}  // namespace snapshot
