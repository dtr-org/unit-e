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

std::vector<uint8_t> PseudoRandomValue(const unsigned int n) {
  std::vector<uint8_t> result;
  for (unsigned int i = 0; i < 4; i++) {
    result.push_back(static_cast<uint8_t>(MurmurHash3(n + i, result) & 0xff));
  }
  return result;
}

BOOST_AUTO_TEST_CASE(test_insert_erase) {
  DefaultIBLT iblt(20);
  iblt.Insert(0, ParseHex("00000000"));
  iblt.Insert(1, ParseHex("00000001"));
  iblt.Insert(11, ParseHex("00000011"));

  std::vector<uint8_t> result;
  BOOST_CHECK(iblt.Get(0, result));
  BOOST_CHECK_EQUAL(HexStr(result), "00000000");
  BOOST_CHECK(iblt.Get(11, result));
  BOOST_CHECK_EQUAL(HexStr(result), "00000011");

  iblt.Erase(0, ParseHex("00000000"));
  iblt.Erase(1, ParseHex("00000001"));
  BOOST_CHECK(iblt.Get(1, result));
  BOOST_CHECK(result.empty());
  iblt.Erase(11, ParseHex("00000011"));
  BOOST_CHECK(iblt.Get(11, result));
  BOOST_CHECK(result.empty());

  iblt.Insert(0, ParseHex("00000000"));
  iblt.Insert(1, ParseHex("00000001"));
  iblt.Insert(11, ParseHex("00000011"));

  for (uint64_t i = 100; i < 115; i++) {
    iblt.Insert(i, ParseHex("aabbccdd"));
  }

  BOOST_CHECK(iblt.Get(101, result));
  BOOST_CHECK_EQUAL(HexStr(result), "aabbccdd");
  BOOST_CHECK(iblt.Get(200, result));
  BOOST_CHECK(result.empty());
}

BOOST_AUTO_TEST_CASE(test_overload) {
  DefaultIBLT iblt(20);

  // 1,000 values in an IBLT that has room for 20,
  // all lookups should fail.
  for (unsigned int i = 0; i < 1000; i++) {
    iblt.Insert(i, PseudoRandomValue(i));
  }

  std::vector<uint8_t> result;
  for (uint64_t i = 0; i < 1000; i += 97) {
    BOOST_CHECK(!iblt.Get(i, result));
    BOOST_CHECK(result.empty());
  }

  // erase all but 20:
  for (unsigned int i = 20; i < 1000; i++) {
    iblt.Erase(i, PseudoRandomValue(i));
  }

  for (unsigned int i = 0; i < 20; i++) {
    BOOST_CHECK(iblt.Get(i, result));
    BOOST_CHECK_EQUAL(HexStr(result), HexStr(PseudoRandomValue(i)));
  }
}

BOOST_AUTO_TEST_CASE(test_list) {
  DefaultIBLT::TEntriesMap expected;
  DefaultIBLT iblt(20);
  for (unsigned int i = 0; i < 20; i++) {
    iblt.Insert(i, PseudoRandomValue(i * 2));
    expected.emplace(i, PseudoRandomValue(i * 2));
  }
  DefaultIBLT::TEntriesMap actual;
  BOOST_CHECK(iblt.ListEntries(actual, actual));
  BOOST_CHECK(actual == expected);
}

BOOST_AUTO_TEST_CASE(test_minus) {
  DefaultIBLT iblt1(11);
  DefaultIBLT iblt2(11);

  for (unsigned int i = 0; i < 195; i++) {
    iblt1.Insert(i, PseudoRandomValue(i));
  }

  for (unsigned int i = 5; i < 200; i++) {
    iblt2.Insert(i, PseudoRandomValue(i));
  }

  DefaultIBLT diff = iblt1 - iblt2;

  // Should end up with 10 differences, 5 positive and 5 negative:
  DefaultIBLT::TEntriesMap expected_positive;
  DefaultIBLT::TEntriesMap expected_negative;
  for (unsigned int i = 0; i < 5; i++) {
    expected_positive.insert(std::make_pair(i, PseudoRandomValue(i)));
    expected_negative.insert(std::make_pair(195 + i, PseudoRandomValue(195 + i)));
  }

  DefaultIBLT::TEntriesMap positive;
  DefaultIBLT::TEntriesMap negative;

  bool decoded = diff.ListEntries(positive, negative);

  BOOST_CHECK(decoded);
  BOOST_CHECK(positive == expected_positive);
  BOOST_CHECK(negative == expected_negative);

  positive.clear();
  negative.clear();
  decoded = (iblt2 - iblt1).ListEntries(positive, negative);
  BOOST_CHECK(decoded);
  BOOST_CHECK(positive == expected_negative);  // Opposite subtraction, opposite results
  BOOST_CHECK(negative == expected_positive);

  DefaultIBLT empty_iblt(11);
  DefaultIBLT::TEntriesMap empty_map;

  // Test edge cases for empty IBLT:
  decoded = empty_iblt.ListEntries(empty_map, empty_map);
  BOOST_CHECK(decoded);
  BOOST_CHECK(empty_map.empty());

  positive.clear();
  negative.clear();
  decoded = (diff - empty_iblt).ListEntries(positive, negative);
  BOOST_CHECK(decoded);
  BOOST_CHECK(positive == expected_positive);
  BOOST_CHECK(negative == expected_negative);

  positive.clear();
  negative.clear();
  decoded = (empty_iblt - diff).ListEntries(positive, negative);
  BOOST_CHECK(decoded);
  BOOST_CHECK(positive == expected_negative);  // Opposite subtraction, opposite results
  BOOST_CHECK(negative == expected_positive);
}

BOOST_AUTO_TEST_SUITE_END()
