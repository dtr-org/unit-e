// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_unite.h>

#include <util.h>
#include <utilfun.h>
#include <functional>
#include <iostream>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(util_fun_tests)

// take_while with different container/element types

BOOST_AUTO_TEST_CASE(take_while_int_vector) {
  std::function<bool(int)> func = [](int c) -> bool { return c < 7; };
  std::vector<int> result =
      take_while(func, std::vector<int>({1, 2, 6, 7, 3, 4, 5, 6, 7, 8, 9}));
  BOOST_CHECK(result == std::vector<int>({1, 2, 6}));
}

BOOST_AUTO_TEST_CASE(take_while_char_vector) {
  std::function<bool(char)> func = [](char c) -> bool { return c % 2 == 0; };
  std::vector<char> result =
      take_while(func, std::vector<char>({'b', 'd', 'a', 'c'}));
  BOOST_CHECK(result == std::vector<char>({'b', 'd'}));
}

BOOST_AUTO_TEST_CASE(take_while_string) {
  std::function<bool(char)> func = [](char c) -> bool { return c < 'd'; };
  std::string result = take_while(func, std::string("abcde"));
  BOOST_CHECK(result == "abc");
}

// drop_while with different container/element types

static bool less_than_seven(int i) { return i < 7; }

BOOST_AUTO_TEST_CASE(drop_while_int_vector_fun_ptr) {
  std::vector<int> result = drop_while(
      &less_than_seven, std::vector<int>({1, 2, 6, 7, 3, 4, 5, 6, 7, 8, 9}));
  BOOST_CHECK(result == std::vector<int>({7, 3, 4, 5, 6, 7, 8, 9}));
}

BOOST_AUTO_TEST_CASE(drop_while_int_vector_lambda) {
  std::vector<int> result =
      drop_while([](int c) -> bool { return c < 7; },
                 std::vector<int>({1, 2, 6, 7, 3, 4, 5, 6, 7, 8, 9}));
  BOOST_CHECK(result == std::vector<int>({7, 3, 4, 5, 6, 7, 8, 9}));
}

BOOST_AUTO_TEST_CASE(drop_while_int_vector) {
  std::function<bool(int)> func = [](int c) -> bool { return c < 7; };
  std::vector<int> result =
      drop_while(func, std::vector<int>({1, 2, 6, 7, 3, 4, 5, 6, 7, 8, 9}));
  BOOST_CHECK(result == std::vector<int>({7, 3, 4, 5, 6, 7, 8, 9}));
}

BOOST_AUTO_TEST_CASE(drop_while_char_vector) {
  std::function<bool(char)> func = [](char c) -> bool { return c % 2 == 0; };
  std::vector<char> result =
      drop_while(func, std::vector<char>({'b', 'd', 'a', 'c'}));
  BOOST_CHECK(result == std::vector<char>({'a', 'c'}));
}

BOOST_AUTO_TEST_CASE(drop_while_string) {
  std::function<bool(char)> func = [](char c) -> bool { return c < 'd'; };
  std::string result = drop_while(func, std::string("abcde"));
  BOOST_CHECK(result == "de");
}

// filter

BOOST_AUTO_TEST_CASE(filter_int_vector) {
  std::function<bool(int)> func = [](int c) -> bool { return c > 5 && c < 8; };
  std::vector<int> result =
      filter(func, std::vector<int>({1, 2, 6, 7, 3, 4, 5, 6, 7, 8, 9}));
  BOOST_CHECK(result == std::vector<int>({6, 7, 6, 7}));
}

// filter_not

BOOST_AUTO_TEST_CASE(filter_not_int_vector) {
  std::function<bool(int)> func = [](int c) -> bool { return c > 5 && c < 8; };
  std::vector<int> result =
      filter_not(func, std::vector<int>({1, 2, 6, 7, 3, 4, 5, 6, 7, 8, 9}));
  BOOST_CHECK(result == std::vector<int>({1, 2, 3, 4, 5, 8, 9}));
}

// zip_with

BOOST_AUTO_TEST_CASE(zip_with_int_vector) {
  auto result =
      zip_with([](int a, int b) -> int { return a + b; },
               std::vector<int>({1, 2, 3}), std::vector<int>({7, 8, 9}));
  BOOST_CHECK(result == std::vector<int>({8, 10, 12}));
}

BOOST_AUTO_TEST_CASE(zip_with_different_lengths) {
  auto result =
      zip_with([](const char a, const int b) { return std::make_pair(a, b); },
               std::vector<char>({'a', 'b'}), std::vector<int>({1, 2, 3}));
  auto expected = std::vector<std::pair<char, int>>(
      {std::make_pair('a', 1), std::make_pair('b', 2)});
  BOOST_CHECK(result == expected);
}

template <class A>
static A plus(A a, A b) {
  return a + b;
}

// fold_left

BOOST_AUTO_TEST_CASE(fold_left_test) {
  auto result = fold_left(&plus<int>, 3, std::vector<int>({3, 5, 8}));
  BOOST_CHECK(result == 19);
}

// fold_right

BOOST_AUTO_TEST_CASE(fold_right_test) {
  auto result = fold_right(&plus<int>, 3, std::vector<int>({3, 5, 8}));
  BOOST_CHECK(result == 19);
}

// scan_left

BOOST_AUTO_TEST_CASE(scan_left_test) {
  auto result = scan_left(&plus<int>, 3, std::vector<int>({3, 5, 8}));
  BOOST_CHECK(result == std::vector<int>({3, 6, 11, 19}));
}

BOOST_AUTO_TEST_SUITE_END()
