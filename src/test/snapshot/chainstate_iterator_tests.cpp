// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/chainstate_iterator.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_chainstate_iterator_tests, BasicTestingSetup)

uint256 uint256FromUint64(uint64_t n) {
  CDataStream s(SER_DISK, PROTOCOL_VERSION);
  s << n;
  s << uint64_t(0);
  s << uint64_t(0);
  s << uint64_t(0);
  uint256 nn;
  s >> nn;
  return nn;
}

BOOST_AUTO_TEST_CASE(chainstate_iterator) {
  SetDataDir("snapshot_chainstate_iterator");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);

  auto view = MakeUnique<CCoinsViewDB>(0, false, true);
  uint64_t totalTxs = 10;
  {
    // generate 10 transactions that every new transaction
    // has one more output than the previous one.
    // tx0(1 output), tx1(2 outputs), txn(n outputs)
    for (uint64_t txId = 0; txId < totalTxs; ++txId) {
      CCoinsMap map;
      for (uint32_t i = 0; i < txId + 1; ++i) {
        COutPoint point;
        point.n = i;
        point.hash = uint256FromUint64(txId);

        CCoinsCacheEntry entry;
        entry.flags |= CCoinsCacheEntry::Flags::DIRTY;
        entry.coin = Coin();
        entry.coin.out.nValue = txId * 100 + i;
        entry.coin.nHeight = static_cast<uint32_t>(txId);
        entry.coin.fCoinBase = static_cast<unsigned int>(txId % 2);

        map[point] = entry;
      }
      BOOST_CHECK(view->BatchWrite(map, uint256S("aa"), {}));
    }
  }

  snapshot::ChainstateIterator iter(view.get());
  uint64_t count = 0;
  while (iter.Valid()) {
    snapshot::UTXOSubset subset = iter.GetUTXOSubset();
    BOOST_CHECK_EQUAL(subset.m_height, count);
    BOOST_CHECK_EQUAL(subset.m_txId.GetUint64(0), count);
    BOOST_CHECK_EQUAL(subset.m_isCoinBase, count % 2 == 1);
    BOOST_CHECK_EQUAL(subset.m_outputs.size(), count + 1);

    int n = 0;
    for (const auto &p : subset.m_outputs) {
      BOOST_CHECK_EQUAL(p.first, n);
      BOOST_CHECK_EQUAL(p.second.nValue, count * 100 + n);
      ++n;
    }

    iter.Next();
    ++count;
  }
  BOOST_CHECK_EQUAL(count, totalTxs);
}

BOOST_AUTO_TEST_SUITE_END()
