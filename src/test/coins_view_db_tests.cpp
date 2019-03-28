// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <coins.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(coins_view_db_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(ccoins_view_cache_clear_coins)
{
  auto view_db = MakeUnique<CCoinsViewDB>(0, true, true);
  auto view_backend = MakeUnique<CCoinsViewCache>(view_db.get());  // extra layer
  auto view_cache = MakeUnique<CCoinsViewCache>(view_backend.get());

  CCoinsMap coins;
  for (size_t i = 0; i < 5; ++i) {
    COutPoint point(uint256S("aa"), static_cast<uint32_t>(i));
    Coin coin(CTxOut(1, CScript()), 1, false);
    CCoinsCacheEntry entry(std::move(coin));
    entry.flags |= CCoinsCacheEntry::DIRTY;
    coins.emplace(point, std::move(entry));
  }
  view_db->BatchWrite(coins, uint256S("aa"), snapshot::SnapshotHash());

  size_t total = 0;
  std::unique_ptr<CCoinsViewCursor> cursor(view_db->Cursor());
  while (cursor->Valid()) {
    ++total;
    cursor->Next();
  }
  BOOST_CHECK_EQUAL(total, 5); // sanity check

  view_cache->ClearCoins();
  std::unique_ptr<CCoinsViewCursor> empty_cursor(view_db->Cursor());
  BOOST_CHECK(!empty_cursor->Valid());
}

BOOST_AUTO_TEST_SUITE_END()
