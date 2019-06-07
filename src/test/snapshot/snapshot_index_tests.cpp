// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/snapshot_index.h>

#include <serialize.h>
#include <streams.h>
#include <test/test_unite.h>
#include <validation.h>
#include <version.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_snapshot_index_tests, ReducedTestingSetup)

bool HasSnapshotHash(snapshot::SnapshotIndex &index, const uint256 &hash) {
  for (const snapshot::Checkpoint &p : index.GetSnapshotCheckpoints()) {
    if (p.snapshot_hash == hash) {
      return true;
    }
  }
  return false;
}

BOOST_AUTO_TEST_CASE(addition) {
  snapshot::SnapshotIndex index(4, 2, true);
  BOOST_CHECK_EQUAL(index.GetSnapshotCheckpoints().size(), 0);

  CBlockIndex block;
  uint256 hash;
  block.phashBlock = &hash;

  // fill the index with non finalized snapshots
  // a2 - a4 - a6 - a8
  for (int count = 1; count < 5; ++count) {
    block.nHeight = count * 2;
    uint256 snapshotHash = uint256S("a" + std::to_string(block.nHeight));
    std::vector<uint256> remove = index.AddSnapshotHash(snapshotHash, &block);
    BOOST_CHECK(remove.empty());
    BOOST_CHECK_EQUAL(index.GetSnapshotCheckpoints().size(), count);
    BOOST_CHECK(HasSnapshotHash(index, snapshotHash));
  }

  // adding the same height replaces existing one
  // a2 - a4 - a6 - a8
  block.nHeight = 4;
  std::vector<uint256> removed = index.AddSnapshotHash(uint256S("c4"), &block);
  BOOST_CHECK_EQUAL(removed.size(), 1);
  BOOST_CHECK_EQUAL(removed[0].GetHex(), uint256S("a4").GetHex());
  BOOST_CHECK(HasSnapshotHash(index, uint256S("c4")));
  BOOST_CHECK(!HasSnapshotHash(index, removed[0]));
  index.ConfirmRemoved(removed[0]);

  // insert in the middle pushes out the highest one
  // a2 - a4 - c5 - a6
  block.nHeight = 5;
  removed = index.AddSnapshotHash(uint256S("c5"), &block);
  BOOST_CHECK_EQUAL(removed.size(), 1);
  BOOST_CHECK_EQUAL(removed[0].GetHex(), uint256S("a8").GetHex());
  BOOST_CHECK(HasSnapshotHash(index, uint256S("c5")));
  BOOST_CHECK(!HasSnapshotHash(index, removed[0]));
  index.ConfirmRemoved(removed[0]);

  // add the lowest pushes out the highest one
  // c1 - a2 - a4 - c5
  block.nHeight = 1;
  removed = index.AddSnapshotHash(uint256S("c1"), &block);
  BOOST_CHECK_EQUAL(removed.size(), 1);
  BOOST_CHECK_EQUAL(removed[0].GetHex(), uint256S("a6").GetHex());
  BOOST_CHECK(HasSnapshotHash(index, uint256S("c1")));
  index.ConfirmRemoved(removed[0]);

  // add the highest pushes out the lowest
  block.nHeight = 7;
  removed = index.AddSnapshotHash(uint256S("c7"), &block);
  BOOST_CHECK_EQUAL(removed.size(), 1);
  BOOST_CHECK_EQUAL(removed[0].GetHex(), uint256S("c1").GetHex());
  BOOST_CHECK(HasSnapshotHash(index, uint256S("c7")));
  index.ConfirmRemoved(removed[0]);
}

BOOST_AUTO_TEST_CASE(finalization) {
  snapshot::SnapshotIndex index(4, 2, true);
  BOOST_CHECK_EQUAL(index.GetSnapshotCheckpoints().size(), 0);

  LOCK(cs_main);

  // create two forks and finalize the first one
  // b0 - b1 - b2 - b3 - b4
  // |
  // +--- b5 - b6 - b7 - b8 - b9 - b10 - b11 - b12 - b13
  auto b0 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b1 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b2 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b3 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b4 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b5 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b6 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b7 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b8 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b9 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b10 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b11 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b12 = std::unique_ptr<CBlockIndex>(new CBlockIndex);
  auto b13 = std::unique_ptr<CBlockIndex>(new CBlockIndex);

  b1->nHeight = 1;
  b2->nHeight = 2;
  b3->nHeight = 3;
  b4->nHeight = 4;

  b4->pprev = b3.get();
  b3->pprev = b2.get();
  b2->pprev = b1.get();
  b1->pprev = b0.get();

  b5->nHeight = 1;
  b6->nHeight = 2;
  b7->nHeight = 3;
  b8->nHeight = 4;
  b9->nHeight = 5;
  b10->nHeight = 6;
  b11->nHeight = 7;
  b12->nHeight = 8;
  b13->nHeight = 9;

  b13->pprev = b12.get();
  b12->pprev = b11.get();
  b11->pprev = b10.get();
  b10->pprev = b9.get();
  b9->pprev = b8.get();
  b8->pprev = b7.get();
  b7->pprev = b6.get();
  b6->pprev = b5.get();
  b5->pprev = b0.get();

  b0->phashBlock = &mapBlockIndex.emplace(uint256S("b0"), b0.get()).first->first;
  b1->phashBlock = &mapBlockIndex.emplace(uint256S("b1"), b1.get()).first->first;
  b2->phashBlock = &mapBlockIndex.emplace(uint256S("b2"), b2.get()).first->first;
  b3->phashBlock = &mapBlockIndex.emplace(uint256S("b3"), b3.get()).first->first;
  b4->phashBlock = &mapBlockIndex.emplace(uint256S("b4"), b4.get()).first->first;
  b5->phashBlock = &mapBlockIndex.emplace(uint256S("b5"), b5.get()).first->first;
  b6->phashBlock = &mapBlockIndex.emplace(uint256S("b6"), b6.get()).first->first;
  b7->phashBlock = &mapBlockIndex.emplace(uint256S("b7"), b7.get()).first->first;
  b8->phashBlock = &mapBlockIndex.emplace(uint256S("b8"), b8.get()).first->first;
  b9->phashBlock = &mapBlockIndex.emplace(uint256S("b9"), b9.get()).first->first;
  b10->phashBlock = &mapBlockIndex.emplace(uint256S("b100"), b10.get()).first->first;
  b11->phashBlock = &mapBlockIndex.emplace(uint256S("b110"), b11.get()).first->first;
  b12->phashBlock = &mapBlockIndex.emplace(uint256S("b120"), b12.get()).first->first;
  b13->phashBlock = &mapBlockIndex.emplace(uint256S("b130"), b13.get()).first->first;

  // c1 - c6 - c3 - c8
  index.AddSnapshotHash(uint256S("c1"), b1.get());
  index.AddSnapshotHash(uint256S("c6"), b6.get());
  index.AddSnapshotHash(uint256S("c3"), b3.get());
  index.AddSnapshotHash(uint256S("c8"), b8.get());

  // finalize removes other forks up to its height
  // c1 - c3 - c8
  std::vector<uint256> removed = index.FinalizeSnapshots(b3.get());
  BOOST_CHECK_EQUAL(removed.size(), 1);
  BOOST_CHECK_EQUAL(removed[0], uint256S("c6"));
  BOOST_CHECK_EQUAL(index.GetSnapshotCheckpoints().size(), 3);
  BOOST_CHECK(HasSnapshotHash(index, uint256S("c1")));
  BOOST_CHECK(HasSnapshotHash(index, uint256S("c3")));
  BOOST_CHECK(HasSnapshotHash(index, uint256S("c8")));
  index.ConfirmRemoved(removed[0]);

  // finalize height=4
  // c1 - c3
  removed = index.FinalizeSnapshots(b4.get());
  BOOST_CHECK_EQUAL(removed.size(), 1);
  BOOST_CHECK_EQUAL(removed[0], uint256S("c8"));
  BOOST_CHECK_EQUAL(index.GetSnapshotCheckpoints().size(), 2);
  index.ConfirmRemoved(removed[0]);

  // add and finalize one more snapshot
  // c1 - c3 - c4
  removed = index.AddSnapshotHash(uint256S("c4"), b4.get());
  BOOST_CHECK(removed.empty());
  removed = index.FinalizeSnapshots(b4.get());
  BOOST_CHECK(removed.empty());

  // adding more snapshots keeps min finalized ones
  // c3 - c4 - c8 - c9
  index.AddSnapshotHash(uint256S("c5"), b9.get());
  index.AddSnapshotHash(uint256S("c6"), b10.get());
  index.AddSnapshotHash(uint256S("c7"), b11.get());
  index.AddSnapshotHash(uint256S("c8"), b12.get());
  removed = index.AddSnapshotHash(uint256S("c9"), b13.get());
  BOOST_CHECK_EQUAL(removed.size(), 4);
  BOOST_CHECK_EQUAL(removed[0], uint256S("c1"));
  BOOST_CHECK_EQUAL(removed[1], uint256S("c5"));
  BOOST_CHECK_EQUAL(removed[2], uint256S("c6"));
  BOOST_CHECK_EQUAL(removed[3], uint256S("c7"));
  BOOST_CHECK_EQUAL(index.GetSnapshotCheckpoints().size(), 4);
  BOOST_CHECK(HasSnapshotHash(index, uint256S("c3")));
  BOOST_CHECK(HasSnapshotHash(index, uint256S("c4")));
  BOOST_CHECK(HasSnapshotHash(index, uint256S("c8")));
  BOOST_CHECK(HasSnapshotHash(index, uint256S("c9")));

  mapBlockIndex.clear();
}

BOOST_AUTO_TEST_CASE(serialization) {
  snapshot::SnapshotIndex index(4, 2, true);

  LOCK(cs_main);

  auto b1 = std::unique_ptr<CBlockIndex>(new CBlockIndex());
  auto b2 = std::unique_ptr<CBlockIndex>(new CBlockIndex());
  auto b3 = std::unique_ptr<CBlockIndex>(new CBlockIndex());
  auto b4 = std::unique_ptr<CBlockIndex>(new CBlockIndex());
  auto b5 = std::unique_ptr<CBlockIndex>(new CBlockIndex());
  auto b6 = std::unique_ptr<CBlockIndex>(new CBlockIndex());
  auto b7 = std::unique_ptr<CBlockIndex>(new CBlockIndex());

  b1->nHeight = 1;
  b2->nHeight = 2;
  b3->nHeight = 3;
  b4->nHeight = 4;
  b5->nHeight = 5;
  b6->nHeight = 6;
  b7->nHeight = 7;

  b7->pprev = b6.get();
  b6->pprev = b5.get();
  b5->pprev = b4.get();
  b4->pprev = b3.get();
  b3->pprev = b2.get();
  b2->pprev = b1.get();

  b1->phashBlock = &mapBlockIndex.emplace(uint256S("b1"), b1.get()).first->first;
  b2->phashBlock = &mapBlockIndex.emplace(uint256S("b2"), b2.get()).first->first;
  b3->phashBlock = &mapBlockIndex.emplace(uint256S("b3"), b3.get()).first->first;
  b4->phashBlock = &mapBlockIndex.emplace(uint256S("b4"), b4.get()).first->first;
  b5->phashBlock = &mapBlockIndex.emplace(uint256S("b5"), b5.get()).first->first;
  b6->phashBlock = &mapBlockIndex.emplace(uint256S("b6"), b6.get()).first->first;
  b7->phashBlock = &mapBlockIndex.emplace(uint256S("b7"), b7.get()).first->first;

  index.AddSnapshotHash(uint256S("c1"), b1.get());
  index.FinalizeSnapshots(b1.get());
  index.AddSnapshotHash(uint256S("c2"), b2.get());
  index.AddSnapshotHash(uint256S("c3"), b3.get());
  index.AddSnapshotHash(uint256S("c4"), b4.get());
  index.AddSnapshotHash(uint256S("c6"), b6.get());
  index.AddSnapshotHash(uint256S("c7"), b7.get());

  CDataStream stream(SER_DISK, PROTOCOL_VERSION);
  stream << index;

  snapshot::SnapshotIndex index2(4, 2, true);
  stream >> index2;
  BOOST_CHECK_EQUAL(index2.GetSnapshotCheckpoints().size(), 4);
  BOOST_CHECK(HasSnapshotHash(index2, uint256S("c1")));
  BOOST_CHECK(HasSnapshotHash(index2, uint256S("c4")));
  BOOST_CHECK(HasSnapshotHash(index2, uint256S("c6")));
  BOOST_CHECK(HasSnapshotHash(index2, uint256S("c7")));

  std::vector<uint256> removed = index2.AddSnapshotHash(uint256S("c5"), b5.get());
  BOOST_CHECK_EQUAL(removed.size(), 3);
  BOOST_CHECK_EQUAL(removed[0], uint256S("c2"));
  BOOST_CHECK_EQUAL(removed[1], uint256S("c3"));
  BOOST_CHECK_EQUAL(removed[2], uint256S("c7"));

  mapBlockIndex.clear();
}

BOOST_AUTO_TEST_SUITE_END()
