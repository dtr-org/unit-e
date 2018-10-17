// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/indexer.h>

#include <crypto/sha256.h>
#include <fs.h>
#include <snapshot/iterator.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_indexer_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(snapshot_indexer_flush) {
  SetDataDir("snapshot_indexer_flush");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);

  uint32_t step = 3;
  uint32_t stepsPerFile = 2;
  std::unique_ptr<snapshot::Indexer> idx(
      new snapshot::Indexer(0, uint256(), uint256(), step, stepsPerFile));
  CDataStream streamIn(SER_DISK, PROTOCOL_VERSION);

  uint64_t totalMsgs = step * stepsPerFile * 3;
  for (uint64_t i = 0; i < totalMsgs; ++i) {
    BOOST_CHECK(idx->Flush());
    snapshot::UTXOSubset subset;
    CDataStream s(SER_DISK, PROTOCOL_VERSION);
    s << i << uint64_t(0) << uint64_t(0) << uint64_t(0);
    s >> subset.m_txId;
    streamIn << subset;
    BOOST_CHECK(idx->WriteUTXOSubset(subset));
  }
  BOOST_CHECK(idx->Flush());

  CDataStream streamOut(SER_DISK, PROTOCOL_VERSION);
  snapshot::Iterator iter(std::move(idx));
  for (uint64_t i = 0; i < totalMsgs; ++i) {
    BOOST_CHECK(iter.MoveCursorTo(i));
    streamOut << iter.GetUTXOSubset();
  }

  BOOST_CHECK_EQUAL(HexStr(streamIn), HexStr(streamOut));
}

BOOST_AUTO_TEST_CASE(snapshot_indexer_writer) {
  SetDataDir("snapshot_indexer_writer");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);

  uint32_t snapshotId = 0;
  uint32_t step = 3;
  uint32_t stepsPerFile = 2;
  uint256 snapshotHash = uint256S("aa");
  snapshot::Indexer indexer(snapshotId, snapshotHash, uint256(), step,
                            stepsPerFile);

  CDataStream stream(SER_DISK, PROTOCOL_VERSION);
  uint64_t totalMsgs = (step * stepsPerFile) * 2 + step;
  for (uint64_t i = 0; i < totalMsgs; ++i) {
    snapshot::UTXOSubset utxoSet;
    stream << utxoSet;
    BOOST_CHECK(indexer.WriteUTXOSubset(utxoSet));
    BOOST_CHECK_EQUAL(indexer.GetMeta().m_totalUTXOSubsets, i + 1);
  }

  fs::path dir = GetDataDir() / "snapshots" / std::to_string(snapshotId);
  BOOST_CHECK(fs::exists(dir / "utxo0.dat"));
  BOOST_CHECK(fs::exists(dir / "utxo1.dat"));
  BOOST_CHECK(!fs::exists(dir / "utxo2.dat"));
  BOOST_CHECK(indexer.Flush());
  BOOST_CHECK(fs::exists(dir / "utxo2.dat"));
  BOOST_CHECK(fs::exists(dir / "meta.dat"));
  BOOST_CHECK(fs::exists(dir / "index.dat"));
  BOOST_CHECK(!fs::exists(dir / "utxo3.dat"));

  BOOST_CHECK_EQUAL(indexer.GetMeta().m_snapshotHash.GetHex(), snapshotHash.GetHex());
}

BOOST_AUTO_TEST_CASE(snapshot_indexer_resume_writing) {
  SetDataDir("snapshot_indexer_resume_writing");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);

  uint32_t snapshotId = 0;
  uint32_t step = 3;
  uint32_t stepsPerFile = 3;
  uint256 snapshotHash = uint256S("aa");
  std::unique_ptr<snapshot::Indexer> indexer(new snapshot::Indexer(
      snapshotId, snapshotHash, uint256(), step, stepsPerFile));

  // close and re-open indexer after each write
  uint64_t totalMsgs = (step * stepsPerFile) * 3 + step;
  CDataStream streamIn(SER_DISK, PROTOCOL_VERSION);
  for (uint64_t i = 0; i < totalMsgs; ++i) {
    CDataStream s(SER_DISK, PROTOCOL_VERSION);
    s << i;
    s << uint64_t(0);
    s << uint64_t(0);
    s << uint64_t(0);
    snapshot::UTXOSubset utxoSet;
    s >> utxoSet.m_txId;

    streamIn << utxoSet;
    BOOST_CHECK(indexer->WriteUTXOSubset(utxoSet));
    BOOST_CHECK_EQUAL(indexer->GetMeta().m_totalUTXOSubsets, i + 1);
    BOOST_CHECK(indexer->Flush());
    indexer = snapshot::Indexer::Open(snapshotId);
    BOOST_CHECK(indexer);
  }

  fs::path dir = GetDataDir() / "snapshots" / std::to_string(snapshotId);
  BOOST_CHECK(fs::exists(dir / "utxo0.dat"));
  BOOST_CHECK(fs::exists(dir / "utxo1.dat"));
  BOOST_CHECK(fs::exists(dir / "utxo2.dat"));
  BOOST_CHECK(fs::exists(dir / "utxo3.dat"));
  BOOST_CHECK(!fs::exists(dir / "utxo4.dat"));

  // validate the content
  indexer = snapshot::Indexer::Open(snapshotId);
  BOOST_CHECK(indexer);

  snapshot::Iterator iter(std::move(indexer));
  CDataStream streamOut(SER_DISK, PROTOCOL_VERSION);
  for (uint64_t i = 0; i < totalMsgs; ++i) {
    BOOST_CHECK(iter.MoveCursorTo(i));
    auto msg = iter.GetUTXOSubset();
    streamOut << msg;
    BOOST_CHECK_EQUAL(msg.m_txId.GetUint64(0), i);
  }
  BOOST_CHECK_EQUAL(HexStr(streamIn), HexStr(streamOut));
  BOOST_CHECK_EQUAL(iter.GetSnapshotHash().GetHex(), snapshotHash.GetHex());
}

BOOST_AUTO_TEST_CASE(snapshot_indexer_open) {
  SetDataDir("snapshot_indexer_open");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);

  uint32_t snapshotId = 0;
  uint32_t step = 3;
  uint32_t stepsPerFile = 2;
  uint256 snapshotHash = uint256S("aa");
  uint256 bestBlockHash = uint256S("bb");

  snapshot::Indexer indexer(snapshotId, snapshotHash, bestBlockHash, step,
                            stepsPerFile);

  uint64_t totalMsgs = (step * stepsPerFile) * 2 + step;
  for (uint64_t i = 0; i < totalMsgs; ++i) {
    BOOST_CHECK(indexer.WriteUTXOSubset(snapshot::UTXOSubset()));
    BOOST_CHECK_EQUAL(indexer.GetMeta().m_totalUTXOSubsets, i + 1);
  }
  BOOST_CHECK(indexer.Flush());

  auto openedIdx = snapshot::Indexer::Open(snapshotId);
  BOOST_CHECK(openedIdx);
  BOOST_CHECK_EQUAL(HexStr(openedIdx->GetMeta().m_snapshotHash),
                    HexStr(snapshotHash));
  BOOST_CHECK_EQUAL(HexStr(openedIdx->GetMeta().m_bestBlockHash),
                    HexStr(bestBlockHash));
  BOOST_CHECK_EQUAL(openedIdx->GetMeta().m_totalUTXOSubsets, totalMsgs);
}

BOOST_AUTO_TEST_SUITE_END()
