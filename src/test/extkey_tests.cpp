// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_unite.h>

#include <extkey.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(extkey_tests)

BOOST_AUTO_TEST_CASE(extkey_path_parse) {
  bool result;
  std::vector<uint32_t> path;
  std::string error;

  result = ParseExtKeyPath(std::string(), path, error);
  BOOST_CHECK_EQUAL(result, false);

  result = ParseExtKeyPath(std::string("m"), path, error);
  BOOST_CHECK_EQUAL(result, true);
  BOOST_CHECK(path.empty());

  result = ParseExtKeyPath(std::string("m/"), path, error);
  BOOST_CHECK_EQUAL(result, false);

  result = ParseExtKeyPath(std::string("m/1"), path, error);
  {
    uint32_t expected[]{1};
    BOOST_CHECK_EQUAL(result, true);
    BOOST_CHECK_EQUAL_COLLECTIONS(path.begin(), path.end(), expected, expected + 1);
  }

  result = ParseExtKeyPath(std::string("1/2/30/400"), path, error);
  {
    uint32_t expected[]{1, 2, 30, 400};
    BOOST_CHECK_EQUAL(result, true);
    BOOST_CHECK_EQUAL_COLLECTIONS(path.begin(), path.end(), expected, expected + 4);
  }

  result = ParseExtKeyPath(std::string("m//1"), path, error);
  BOOST_CHECK_EQUAL(result, false);

  result = ParseExtKeyPath(std::string("m/1h"), path, error);
  {
    uint32_t expected[]{BIP32_HARDENED_KEY_LIMIT + 1};
    BOOST_CHECK_EQUAL(result, true);
    BOOST_CHECK_EQUAL_COLLECTIONS(path.begin(), path.end(), expected, expected + 1);
  }

  result = ParseExtKeyPath(std::string("m/1'"), path, error);
  {
    uint32_t expected[]{BIP32_HARDENED_KEY_LIMIT + 1};
    BOOST_CHECK_EQUAL(result, true);
    BOOST_CHECK_EQUAL_COLLECTIONS(path.begin(), path.end(), expected, expected + 1);
  }

  result = ParseExtKeyPath(std::string("m/1'/2''"), path, error);
  BOOST_CHECK_EQUAL(result, false);
}

BOOST_AUTO_TEST_CASE(extkey_path_format) {
  std::vector<uint32_t> path = {
      BIP32_HARDENED_KEY_LIMIT + 44, BIP32_HARDENED_KEY_LIMIT + 1,
      BIP32_HARDENED_KEY_LIMIT, 1, 0};

  std::string path_str = FormatExtKeyPath(path);
  BOOST_CHECK(path_str == "m/44'/1'/0'/1/0");
}

BOOST_AUTO_TEST_SUITE_END()
