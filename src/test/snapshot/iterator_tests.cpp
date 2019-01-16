// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/iterator.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_iterator_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(snapshot_iterator) {
  SetDataDir("snapshot_iterator");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);

  uint32_t msgsToGenerate = 20;
  snapshot::SnapshotHash snapshotHash;

  {
    // generate the snapshot
    uint256 blockHash = uint256S("aa");
    snapshot::Indexer idx(uint256S("bb"), blockHash, uint256(), uint256(), 3, 2);
    for (uint32_t i = 0; i < msgsToGenerate; ++i) {
      snapshot::UTXOSubset subset;
      subset.tx_id.SetHex(std::to_string(i));

      CTxOut out;
      out.nValue = 1000 + i;
      subset.outputs[i] = out;
      idx.WriteUTXOSubset(subset);
      snapshotHash.AddUTXO(
          snapshot::UTXO(COutPoint(subset.tx_id, i), Coin(out, 0, false)));
    }
    BOOST_CHECK(idx.Flush());
  }

  {
    // test snapshot hash calculation
    std::unique_ptr<snapshot::Indexer> idx = snapshot::Indexer::Open(uint256S("bb"));
    BOOST_CHECK(idx != nullptr);
    snapshot::Iterator iter(std::move(idx));
    BOOST_CHECK_EQUAL(iter.CalculateHash(uint256(), uint256()).GetHex(),
                      snapshotHash.GetHash(uint256(), uint256()).GetHex());
  }

  {
    // open the snapshot
    auto idx = snapshot::Indexer::Open(uint256S("bb"));
    BOOST_CHECK(idx != nullptr);

    snapshot::Iterator iter(std::move(idx));
    BOOST_CHECK_EQUAL(
        HexStr(iter.GetBestBlockHash()),
        "aa00000000000000000000000000000000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(iter.GetTotalUTXOSubsets(), msgsToGenerate);

    // iterate sequentially
    uint32_t count = 0;
    while (iter.Valid()) {
      uint32_t value = 1000 + count;
      BOOST_CHECK_EQUAL(iter.GetUTXOSubset().outputs.at(count).nValue, value);
      iter.Next();
      ++count;
    }
    BOOST_CHECK_EQUAL(count, msgsToGenerate);

    // iterate via cursor moving forward
    for (uint32_t i = 0; i < msgsToGenerate; ++i) {
      BOOST_CHECK(iter.MoveCursorTo(i));
      int value = 1000 + i;
      BOOST_CHECK_EQUAL(iter.GetUTXOSubset().outputs.at(i).nValue, value);
    }

    // iterate via cursor moving backward
    for (uint32_t i = msgsToGenerate; i > 0; --i) {
      BOOST_CHECK(iter.MoveCursorTo(i - 1));

      uint32_t value = 1000 + i - 1;
      BOOST_CHECK_EQUAL(iter.GetUTXOSubset().outputs.at(i - 1).nValue, value);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
