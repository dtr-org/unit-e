// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <snapshot/creator.h>

#include <algorithm>

#include <snapshot/indexer.h>
#include <snapshot/iterator.h>
#include <test/test_unite.h>
#include <util.h>
#include <validation.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_creator_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(snapshot_creator) {
  SetDataDir("snapshot_creator");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);

  uint256 bestBlock = uint256S("aa");
  uint256 stakeModifier = uint256S("bb");
  auto bi = new CBlockIndex();
  bi->nTime = 1269211443;
  bi->nBits = 246;
  bi->bnStakeModifier = stakeModifier;
  mapBlockIndex[bestBlock] = bi;

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

  snapshot::Creator creator(viewDB.get());
  creator.m_step = 3;
  creator.m_stepsPerFile = 2;

  {
    // create snapshots
    std::vector<uint32_t> ids;
    for (uint32_t idx = 0; idx < 10; ++idx) {
      snapshot::CreationInfo info = creator.Create();

      // validate snapshot ID
      uint32_t snapshotId{0};
      BOOST_CHECK(viewDB->GetSnapshotId(snapshotId));
      BOOST_CHECK_EQUAL(snapshotId, idx);
      ids.emplace_back(idx);

      // keep up to 5 snapshots
      auto lastN = std::min<uint64_t>(5, ids.size());

      BOOST_CHECK(viewDB->GetSnapshotIds() ==
                  std::vector<uint32_t>(ids.end() - lastN, ids.end()));

      // validate reported state
      BOOST_CHECK_EQUAL(info.m_status, +snapshot::Status::OK);
      BOOST_CHECK(!info.m_indexerMeta.m_snapshotHash.IsNull());
      BOOST_CHECK_EQUAL(info.m_indexerMeta.m_snapshotHash.GetHex(),
                        viewDB->GetSnapshotHash().GetHash(stakeModifier).GetHex());
      BOOST_CHECK_EQUAL(HexStr(info.m_indexerMeta.m_bestBlockHash),
                        HexStr(bestBlock));
      BOOST_CHECK_EQUAL(info.m_indexerMeta.m_totalUTXOSubsets, totalTX);
      BOOST_CHECK_EQUAL(info.m_totalOutputs,
                        static_cast<int>(totalTX * coinsPerTX));

      // validate snapshot content
      auto i = snapshot::Indexer::Open(snapshotId);
      snapshot::Iterator iter(std::move(i));
      uint64_t count = 0;
      while (iter.Valid()) {
        ++count;
        iter.Next();
      }
      BOOST_CHECK_EQUAL(info.m_indexerMeta.m_totalUTXOSubsets, count);
    }
  }

  // cleanup as this test has side effects
  UnloadBlockIndex();
}

BOOST_AUTO_TEST_SUITE_END()
