// Copyright (c) 2019 The Unit-e developers
// Copyright (c) 2014 Gavin Andresen
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cassert>
#include <iostream>

#include <hash.h>
#include <iblt.h>

#include <test/test_unite.h>
#include <uint256.h>
#include <util.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(iblt_tests, ReducedTestingSetup)

using DefaultIBLT = IBLT<uint64_t, int32_t, 4>;

std::vector<uint8_t> PseudoRandomValue(uint32_t n) {
  std::vector<uint8_t> result;
  for (int i = 0; i < 4; i++) {
    result.push_back(static_cast<uint8_t>(MurmurHash3(n + i, result) & 0xff));
  }
  return result;
}

BOOST_AUTO_TEST_CASE(test_insert_erase) {
  DefaultIBLT t(20);
  t.Insert(0, ParseHex("00000000"));
  t.Insert(1, ParseHex("00000001"));
  t.Insert(11, ParseHex("00000011"));

  std::vector<uint8_t> result;
  BOOST_CHECK(t.Get(0, result));
  BOOST_CHECK_EQUAL(HexStr(result), "00000000");
  BOOST_CHECK(t.Get(11, result));
  BOOST_CHECK_EQUAL(HexStr(result), "00000011");

  t.Erase(0, ParseHex("00000000"));
  t.Erase(1, ParseHex("00000001"));
  BOOST_CHECK(t.Get(1, result));
  BOOST_CHECK(result.empty());
  t.Erase(11, ParseHex("00000011"));
  BOOST_CHECK(t.Get(11, result));
  BOOST_CHECK(result.empty());

  t.Insert(0, ParseHex("00000000"));
  t.Insert(1, ParseHex("00000001"));
  t.Insert(11, ParseHex("00000011"));

  for (uint64_t i = 100; i < 115; i++) {
    t.Insert(i, ParseHex("aabbccdd"));
  }

  BOOST_CHECK(t.Get(101, result));
  BOOST_CHECK_EQUAL(HexStr(result), "aabbccdd");
  BOOST_CHECK(t.Get(200, result));
  BOOST_CHECK(result.empty());
}

BOOST_AUTO_TEST_CASE(test_overload) {
  DefaultIBLT t(20);

  // 1,000 values in an IBLT that has room for 20,
  // all lookups should fail.
  for (uint32_t i = 0; i < 1000; i++) {
    t.Insert(i, PseudoRandomValue(i));
  }

  std::vector<uint8_t> result;
  for (uint64_t i = 0; i < 1000; i += 97) {
    BOOST_CHECK(!t.Get(i, result));
    BOOST_CHECK(result.empty());
  }

  // erase all but 20:
  for (uint32_t i = 20; i < 1000; i++) {
    t.Erase(i, PseudoRandomValue(i));
  }

  for (int i = 0; i < 20; i++) {
    BOOST_CHECK(t.Get(i, result));
    BOOST_CHECK_EQUAL(HexStr(result), HexStr(PseudoRandomValue(i)));
  }
}

BOOST_AUTO_TEST_CASE(test_list) {
  DefaultIBLT::TEntriesMap expected;
  DefaultIBLT t(20);
  for (uint32_t i = 0; i < 20; i++) {
    t.Insert(i, PseudoRandomValue(i * 2));
    expected.emplace(i, PseudoRandomValue(i * 2));
  }
  DefaultIBLT::TEntriesMap actual;
  BOOST_CHECK(t.ListEntries(actual, actual));
  BOOST_CHECK(actual == expected);
}

BOOST_AUTO_TEST_CASE(test_minus) {
  DefaultIBLT t1(11);
  DefaultIBLT t2(11);

  for (uint32_t i = 0; i < 195; i++) {
    t1.Insert(i, PseudoRandomValue(i));
  }
  for (uint32_t i = 5; i < 200; i++) {
    t2.Insert(i, PseudoRandomValue(i));
  }

  DefaultIBLT diff = t1 - t2;

  // Should end up with 10 differences, 5 positive and 5 negative:
  DefaultIBLT::TEntriesMap expectedPositive;
  DefaultIBLT::TEntriesMap expectedNegative;
  for (uint32_t i = 0; i < 5; i++) {
    expectedPositive.insert(std::make_pair(i, PseudoRandomValue(i)));
    expectedNegative.insert(std::make_pair(195 + i, PseudoRandomValue(195 + i)));
  }
  DefaultIBLT::TEntriesMap positive;
  DefaultIBLT::TEntriesMap negative;
  bool allDecoded = diff.ListEntries(positive, negative);
  BOOST_CHECK(allDecoded);
  BOOST_CHECK(positive == expectedPositive);
  BOOST_CHECK(negative == expectedNegative);

  positive.clear();
  negative.clear();
  allDecoded = (t2 - t1).ListEntries(positive, negative);
  BOOST_CHECK(allDecoded);
  BOOST_CHECK(positive == expectedNegative);  // Opposite subtraction, opposite results
  BOOST_CHECK(negative == expectedPositive);

  DefaultIBLT emptyIBLT(11);
  DefaultIBLT::TEntriesMap emptySet;

  // Test edge cases for empty IBLT:
  allDecoded = emptyIBLT.ListEntries(emptySet, emptySet);
  BOOST_CHECK(allDecoded);
  BOOST_CHECK(emptySet.empty());

  positive.clear();
  negative.clear();
  allDecoded = (diff - emptyIBLT).ListEntries(positive, negative);
  BOOST_CHECK(allDecoded);
  BOOST_CHECK(positive == expectedPositive);
  BOOST_CHECK(negative == expectedNegative);

  positive.clear();
  negative.clear();
  allDecoded = (emptyIBLT - diff).ListEntries(positive, negative);
  BOOST_CHECK(allDecoded);
  BOOST_CHECK(positive == expectedNegative);  // Opposite subtraction, opposite results
  BOOST_CHECK(negative == expectedPositive);
}

BOOST_AUTO_TEST_SUITE_END()
