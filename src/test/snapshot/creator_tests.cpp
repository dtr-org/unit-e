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
      BOOST_CHECK(!info.indexer_meta.snapshot_hash.IsNull());
      BOOST_CHECK_EQUAL(info.indexer_meta.snapshot_hash.GetHex(),
                        checkpoints.rbegin()->snapshot_hash.GetHex());
      BOOST_CHECK_EQUAL(HexStr(info.indexer_meta.block_hash),
                        HexStr(bestBlock));
      BOOST_CHECK_EQUAL(info.indexer_meta.total_utxo_subsets, totalTX);
      BOOST_CHECK_EQUAL(info.total_outputs,
                        static_cast<int>(totalTX * coinsPerTX));

      // validate snapshot content
      auto i = snapshot::Indexer::Open(checkpoints.rbegin()->snapshot_hash);
      snapshot::Iterator iter(std::move(i));
      uint64_t count = 0;
      while (iter.Valid()) {
        ++count;
        iter.Next();
      }
      BOOST_CHECK_EQUAL(info.indexer_meta.total_utxo_subsets, count);
    }

    BOOST_CHECK_EQUAL(deletedSnapshots.size(), 5);
    for (const uint256 &hash : deletedSnapshots) {
      BOOST_CHECK(!HasSnapshotHash(hash));
      BOOST_CHECK(snapshot::Indexer::Open(hash) == nullptr);
    }
  }

  // cleanup as this test has side effects
  UnloadBlockIndex();
}

BOOST_AUTO_TEST_SUITE_END()
