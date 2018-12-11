// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <fixed_vector.h>
#include <test/test_unite.h>

#include <boost/test/unit_test.hpp>

#include <set>

namespace {
struct X {
  int one;
  int two;
  X(int one, int two) : one(one), two(two) {}
};
}  // namespace

BOOST_AUTO_TEST_SUITE(fixed_vector_tests)

BOOST_AUTO_TEST_CASE(check_vector) {
  FixedVector<X> v(3);
  BOOST_CHECK(v.empty());
  BOOST_CHECK_EQUAL(v.size(), 0);
  BOOST_CHECK_EQUAL(v.capacity(), 3);
  BOOST_CHECK_EQUAL(v.remaining(), 3);
}

BOOST_AUTO_TEST_CASE(check_emplace_back) {
  FixedVector<X> v(4);
  auto x = v.emplace_back(2, 7);
  auto y = v.emplace_back(5, 3);
  auto z = v.emplace_back(9, 4);
  BOOST_CHECK(!v.empty());
  BOOST_CHECK_EQUAL(v.size(), 3);
  BOOST_CHECK_EQUAL(v.capacity(), 4);
  BOOST_CHECK_EQUAL(v.remaining(), 1);
  BOOST_CHECK_EQUAL(x.one, 2);
  BOOST_CHECK_EQUAL(x.two, 7);
  BOOST_CHECK_EQUAL(y.one, 5);
  BOOST_CHECK_EQUAL(y.two, 3);
  BOOST_CHECK_EQUAL(z.one, 9);
  BOOST_CHECK_EQUAL(z.two, 4);
}

BOOST_AUTO_TEST_CASE(check_push_back_copy) {
  FixedVector<X> v(2);
  X x(2, 7);
  auto x2 = v.push_back(x);
  BOOST_CHECK(!v.empty());
  BOOST_CHECK_EQUAL(v.size(), 1);
  BOOST_CHECK_EQUAL(v.capacity(), 2);
  BOOST_CHECK_EQUAL(v.remaining(), 1);
  BOOST_CHECK_EQUAL(x2.one, 2);
  BOOST_CHECK_EQUAL(x2.two, 7);
  BOOST_CHECK(&x != &x2);
}

BOOST_AUTO_TEST_CASE(check_push_back_move) {
  FixedVector<X> v(2);
  X x(2, 7);
  auto x2 = v.push_back(std::move(x));
  BOOST_CHECK(!v.empty());
  BOOST_CHECK_EQUAL(v.size(), 1);
  BOOST_CHECK_EQUAL(v.capacity(), 2);
  BOOST_CHECK_EQUAL(v.remaining(), 1);
  BOOST_CHECK_EQUAL(x2.one, 2);
  BOOST_CHECK_EQUAL(x2.two, 7);
  BOOST_CHECK(&x != &x2);
}

BOOST_AUTO_TEST_CASE(check_pop) {
  FixedVector<X> v(2);
  v.emplace_back(2, 7);
  v.emplace_back(5, 3);
  BOOST_CHECK_EQUAL(v.size(), 2);
  BOOST_CHECK_EQUAL(v.capacity(), 2);
  BOOST_CHECK_EQUAL(v.remaining(), 0);
  BOOST_CHECK(v.pop());
  BOOST_CHECK_EQUAL(v.size(), 1);
  BOOST_CHECK_EQUAL(v.capacity(), 2);
  BOOST_CHECK_EQUAL(v.remaining(), 1);
  BOOST_CHECK(v.pop());
  BOOST_CHECK_EQUAL(v.size(), 0);
  BOOST_CHECK_EQUAL(v.capacity(), 2);
  BOOST_CHECK_EQUAL(v.remaining(), 2);
  BOOST_CHECK(v.empty());
}

BOOST_AUTO_TEST_SUITE_END()
