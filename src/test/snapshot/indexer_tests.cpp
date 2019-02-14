// Copyright (c) 2018-2019 The Unit-e developers
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
  uint32_t steps_per_file = 2;
  auto idx = MakeUnique<snapshot::Indexer>(snapshot::SnapshotHeader(), step, steps_per_file);
  CDataStream stream_in(SER_DISK, PROTOCOL_VERSION);

  uint64_t total_msgs = step * steps_per_file * 3;
  for (uint64_t i = 0; i < total_msgs; ++i) {
    BOOST_CHECK(idx->Flush());
    snapshot::UTXOSubset subset;
    CDataStream s(SER_DISK, PROTOCOL_VERSION);
    s << i << uint64_t(0) << uint64_t(0) << uint64_t(0);
    s >> subset.tx_id;
    stream_in << subset;
    BOOST_CHECK(idx->WriteUTXOSubset(subset));
  }
  BOOST_CHECK(idx->Flush());

  CDataStream stream_out(SER_DISK, PROTOCOL_VERSION);
  snapshot::Iterator iter(std::move(idx));
  for (uint64_t i = 0; i < total_msgs; ++i) {
    BOOST_CHECK(iter.MoveCursorTo(i));
    stream_out << iter.GetUTXOSubset();
  }

  BOOST_CHECK_EQUAL(HexStr(stream_in), HexStr(stream_out));
}

BOOST_AUTO_TEST_CASE(snapshot_indexer_writer) {
  SetDataDir("snapshot_indexer_writer");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);

  uint32_t step = 3;
  uint32_t steps_per_file = 2;
  uint256 snapshot_hash = uint256S("aa");
  snapshot::SnapshotHeader snapshot_header;
  snapshot_header.snapshot_hash = snapshot_hash;
  snapshot::Indexer indexer(snapshot_header, step, steps_per_file);

  CDataStream stream(SER_DISK, PROTOCOL_VERSION);
  uint64_t total_msgs = (step * steps_per_file) * 2 + step;
  for (uint64_t i = 0; i < total_msgs; ++i) {
    snapshot::UTXOSubset utxo_subset;
    stream << utxo_subset;
    BOOST_CHECK(indexer.WriteUTXOSubset(utxo_subset));
    BOOST_CHECK_EQUAL(indexer.GetSnapshotHeader().total_utxo_subsets, i + 1);
  }

  fs::path dir = GetDataDir() / "snapshots" / snapshot_hash.GetHex();
  BOOST_CHECK(fs::exists(dir / "utxo0.dat"));
  BOOST_CHECK(fs::exists(dir / "utxo1.dat"));
  BOOST_CHECK(!fs::exists(dir / "utxo2.dat"));
  BOOST_CHECK(indexer.Flush());
  BOOST_CHECK(fs::exists(dir / "utxo2.dat"));
  BOOST_CHECK(fs::exists(dir / "meta.dat"));
  BOOST_CHECK(fs::exists(dir / "index.dat"));
  BOOST_CHECK(!fs::exists(dir / "utxo3.dat"));

  BOOST_CHECK_EQUAL(indexer.GetSnapshotHeader().snapshot_hash.GetHex(),
                    snapshot_header.snapshot_hash.GetHex());
}

BOOST_AUTO_TEST_CASE(snapshot_indexer_resume_writing) {
  SetDataDir("snapshot_indexer_resume_writing");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);

  uint32_t step = 3;
  uint32_t steps_per_file = 3;
  uint256 snapshot_hash = uint256S("aa");
  snapshot::SnapshotHeader snapshot_header;
  snapshot_header.snapshot_hash = snapshot_hash;
  auto indexer = MakeUnique<snapshot::Indexer>(snapshot_header, step, steps_per_file);

  // close and re-open indexer after each write
  uint64_t total_msgs = (step * steps_per_file) * 3 + step;
  CDataStream stream_in(SER_DISK, PROTOCOL_VERSION);
  for (uint64_t i = 0; i < total_msgs; ++i) {
    CDataStream s(SER_DISK, PROTOCOL_VERSION);
    s << i;
    s << uint64_t(0);
    s << uint64_t(0);
    s << uint64_t(0);
    snapshot::UTXOSubset utxo_subset;
    s >> utxo_subset.tx_id;

    stream_in << utxo_subset;
    BOOST_CHECK(indexer->WriteUTXOSubset(utxo_subset));
    BOOST_CHECK_EQUAL(indexer->GetSnapshotHeader().total_utxo_subsets, i + 1);
    BOOST_CHECK(indexer->Flush());
    LOCK(snapshot::cs_snapshot);
    indexer = snapshot::Indexer::Open(snapshot_hash);
    BOOST_CHECK(indexer);
  }

  fs::path dir = GetDataDir() / "snapshots" / snapshot_hash.GetHex();
  BOOST_CHECK(fs::exists(dir / "utxo0.dat"));
  BOOST_CHECK(fs::exists(dir / "utxo1.dat"));
  BOOST_CHECK(fs::exists(dir / "utxo2.dat"));
  BOOST_CHECK(fs::exists(dir / "utxo3.dat"));
  BOOST_CHECK(!fs::exists(dir / "utxo4.dat"));

  // validate the content
  LOCK(snapshot::cs_snapshot);
  indexer = snapshot::Indexer::Open(snapshot_hash);
  BOOST_CHECK(indexer);

  snapshot::Iterator iter(std::move(indexer));
  CDataStream stream_out(SER_DISK, PROTOCOL_VERSION);
  for (uint64_t i = 0; i < total_msgs; ++i) {
    BOOST_CHECK(iter.MoveCursorTo(i));
    auto msg = iter.GetUTXOSubset();
    stream_out << msg;
    BOOST_CHECK_EQUAL(msg.tx_id.GetUint64(0), i);
  }
  BOOST_CHECK_EQUAL(HexStr(stream_in), HexStr(stream_out));
  BOOST_CHECK_EQUAL(iter.GetSnapshotHeader().snapshot_hash.GetHex(),
                    snapshot_hash.GetHex());
}

BOOST_AUTO_TEST_CASE(snapshot_indexer_open) {
  SetDataDir("snapshot_indexer_open");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);

  snapshot::SnapshotHeader snapshot_header;
  snapshot_header.snapshot_hash = uint256S("aa");
  snapshot_header.block_hash = uint256S("bb");
  snapshot_header.stake_modifier = uint256S("cc");
  snapshot_header.chain_stake = uint256S("dd");
  uint32_t step = 3;
  uint32_t steps_per_file = 2;

  snapshot::Indexer indexer(snapshot_header, step, steps_per_file);

  uint64_t total_msgs = (step * steps_per_file) * 2 + step;
  for (uint64_t i = 0; i < total_msgs; ++i) {
    BOOST_CHECK(indexer.WriteUTXOSubset(snapshot::UTXOSubset()));
    BOOST_CHECK_EQUAL(indexer.GetSnapshotHeader().total_utxo_subsets, i + 1);
  }
  BOOST_CHECK(indexer.Flush());

  LOCK(snapshot::cs_snapshot);
  auto opened_idx = snapshot::Indexer::Open(snapshot_header.snapshot_hash);
  BOOST_CHECK(opened_idx);
  BOOST_CHECK_EQUAL(HexStr(opened_idx->GetSnapshotHeader().snapshot_hash),
                    HexStr(snapshot_header.snapshot_hash));
  BOOST_CHECK_EQUAL(HexStr(opened_idx->GetSnapshotHeader().block_hash),
                    HexStr(snapshot_header.block_hash));
  BOOST_CHECK_EQUAL(HexStr(opened_idx->GetSnapshotHeader().stake_modifier),
                    HexStr(snapshot_header.stake_modifier));
  BOOST_CHECK_EQUAL(HexStr(opened_idx->GetSnapshotHeader().chain_stake),
                    HexStr(snapshot_header.chain_stake));
  BOOST_CHECK_EQUAL(opened_idx->GetSnapshotHeader().total_utxo_subsets, total_msgs);
}

BOOST_AUTO_TEST_SUITE_END()
