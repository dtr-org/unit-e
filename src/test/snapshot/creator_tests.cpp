// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <snapshot/creator.h>

#include <algorithm>

#include <snapshot/indexer.h>
#include <snapshot/iterator.h>
#include <snapshot/snapshot_index.h>
#include <test/test_unite.h>
#include <util.h>
#include <validation.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_creator_tests, BasicTestingSetup)

bool HasSnapshotHash(const uint256 &hash) {
  for (const snapshot::Checkpoint &p : snapshot::GetSnapshotCheckpoints()) {
    if (p.snapshot_hash == hash) {
      return true;
    }
  }
  return false;
}

BOOST_AUTO_TEST_CASE(snapshot_creator) {
  SetDataDir("snapshot_creator");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);
  assert(snapshot::GetSnapshotCheckpoints().empty());

  uint256 bestBlock = uint256S("aa");
  auto bi = new CBlockIndex();
  bi->nTime = 1269211443;
  bi->nBits = 246;
  bi->phashBlock = &mapBlockIndex.emplace(bestBlock, bi).first->first;

  auto viewDB = MakeUnique<CCoinsViewDB>(0, false, true);
  auto viewCache = MakeUnique<CCoinsViewCache>(viewDB.get());
  viewCache->SetBestBlock(bestBlock);

  const uint32_t totalTX = 100;
  const uint32_t coinsPerTX = 2;

  {
    // generate Coins in chainstate
    CCoinsMap map;
    for (uint32_t i = 0; i < totalTX * coinsPerTX; ++i) {
      COutPoint point;
      point.n = i;
      CDataStream s(SER_DISK, PROTOCOL_VERSION);
      s << i / coinsPerTX;
      point.hash.SetHex(HexStr(s));

      Coin coin(CTxOut(1, CScript()), 1, false);
      viewCache->AddCoin(point, std::move(coin), false);
    }
    BOOST_CHECK(viewCache->Flush());
  }

  {
    // create snapshots
    std::vector<uint256> deletedSnapshots;
    size_t maxSnapshotsToKeep = 5;
    for (uint32_t idx = 0; idx < 10; ++idx) {
      // update stake modifier to trigger different snapshot hash
      std::string sm = "a" + std::to_string(idx);
      mapBlockIndex[bestBlock]->stake_modifier.SetHex(sm);
      mapBlockIndex[bestBlock]->nHeight = idx;

      snapshot::Creator creator(viewDB.get());
      creator.m_step = 3;
      creator.m_steps_per_file = 2;
      snapshot::CreationInfo info = creator.Create();

      std::vector<snapshot::Checkpoint> checkpoints =
          snapshot::GetSnapshotCheckpoints();
      BOOST_CHECK(!checkpoints.empty());
      BOOST_CHECK(checkpoints.size() <= maxSnapshotsToKeep);

      if (idx == 4) {
        for (const snapshot::Checkpoint &p : checkpoints) {
          deletedSnapshots.push_back(p.snapshot_hash);
        }
      }

      // validate reported state
      BOOST_CHECK_EQUAL(info.status, +snapshot::Status::OK);
      BOOST_CHECK(!info.snapshot_header.snapshot_hash.IsNull());
      BOOST_CHECK_EQUAL(info.snapshot_header.snapshot_hash.GetHex(),
                        checkpoints.rbegin()->snapshot_hash.GetHex());
      BOOST_CHECK_EQUAL(HexStr(info.snapshot_header.block_hash),
                        HexStr(bestBlock));
      BOOST_CHECK_EQUAL(info.snapshot_header.total_utxo_subsets, totalTX);
      BOOST_CHECK_EQUAL(info.total_outputs,
                        static_cast<int>(totalTX * coinsPerTX));

      // validate snapshot content
      uint64_t count = 0;
      {
        LOCK(snapshot::cs_snapshot);
        auto i = snapshot::Indexer::Open(checkpoints.rbegin()->snapshot_hash);
        snapshot::Iterator iter(std::move(i));
        while (iter.Valid()) {
          ++count;
          iter.Next();
        }
      }
      BOOST_CHECK_EQUAL(info.snapshot_header.total_utxo_subsets, count);
    }

    BOOST_CHECK_EQUAL(deletedSnapshots.size(), 5);
    for (const uint256 &hash : deletedSnapshots) {
      BOOST_CHECK(!HasSnapshotHash(hash));
      LOCK(snapshot::cs_snapshot);
      BOOST_CHECK(snapshot::Indexer::Open(hash) == nullptr);
    }
  }

  // cleanup as this test has side effects
  UnloadBlockIndex();
}

BOOST_AUTO_TEST_CASE(snapshot_creator_concurrent_read) {
  SetDataDir("snapshot_creator_multithreading");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);
  assert(snapshot::GetSnapshotCheckpoints().empty());

  uint256 bestBlock = uint256S("aa");
  auto bi = new CBlockIndex();
  bi->nTime = 1269211443;
  bi->nBits = 246;
  bi->phashBlock = &mapBlockIndex.emplace(bestBlock, bi).first->first;

  auto viewDB = MakeUnique<CCoinsViewDB>(0, false, true);
  auto viewCache = MakeUnique<CCoinsViewCache>(viewDB.get());
  viewCache->SetBestBlock(bestBlock);

  const uint32_t totalTX = 100;
  const uint32_t coinsPerTX = 2;

  {
    // generate Coins in chainstate
    for (uint32_t i = 0; i < totalTX * coinsPerTX; ++i) {
      COutPoint point;
      point.n = i;
      CDataStream s(SER_DISK, PROTOCOL_VERSION);
      s << i / coinsPerTX;
      point.hash.SetHex(HexStr(s));

      Coin coin(CTxOut(1, CScript()), 1, false);
      viewCache->AddCoin(point, std::move(coin), false);
    }
    BOOST_CHECK(viewCache->Flush());
  }

  // generate one snapshot
  snapshot::Creator creator(viewDB.get());
  BOOST_CHECK_EQUAL(creator.Create().status, +snapshot::Status::OK);
  BOOST_CHECK_EQUAL(snapshot::GetSnapshotCheckpoints().size(), 1);

  std::atomic<bool> stop_thread(false);
  std::thread read_snapshot_thread([&] {
    while (!stop_thread) {
      LOCK(snapshot::cs_snapshot);
      snapshot::Checkpoint p = snapshot::GetSnapshotCheckpoints()[0];
      std::unique_ptr<snapshot::Indexer> indexer = snapshot::Indexer::Open(p.snapshot_hash);
      snapshot::Iterator iter(std::move(indexer));
      while (iter.Valid()) {
        iter.Next();
      }
    }
  });

  snapshot::Checkpoint prev_point = snapshot::GetSnapshotCheckpoints()[0];
  for (int i = 0; i < 50; ++i) {
    // update chainstate to produce new snapshot hash
    COutPoint point;
    point.n = 5000 + static_cast<uint32_t>(i);
    Coin coin(CTxOut(1, CScript()), 1, false);
    viewCache->AddCoin(point, std::move(coin), false);
    BOOST_CHECK(viewCache->Flush());

    snapshot::Creator cr(viewDB.get());
    BOOST_CHECK_EQUAL(cr.Create().status, +snapshot::Status::OK);

    // ensures new snapshots are created
    snapshot::Checkpoint new_point = snapshot::GetSnapshotCheckpoints()[0];
    BOOST_CHECK(new_point.snapshot_hash != prev_point.snapshot_hash);
    prev_point = new_point;
  }

  stop_thread = true;
  read_snapshot_thread.join();

  // cleanup as this test has side effects
  UnloadBlockIndex();
}

BOOST_AUTO_TEST_SUITE_END()
