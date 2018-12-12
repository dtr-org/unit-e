// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <fixed_vector.h>
#include <test/test_unite.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <type_traits>

namespace {

struct MovableOnly {
  int one;
  int two;
  MovableOnly(int one, int two) : one(one), two(two) {}
  MovableOnly(const MovableOnly &) = delete;
  MovableOnly(MovableOnly &&) = default;
};

static_assert(!std::is_copy_constructible<MovableOnly>::value, "MovableOnly must not be CopyConstructible");
static_assert(std::is_move_constructible<MovableOnly>::value, "MovableOnly must be MoveConstructible");

struct CopyableOnly {
  int one;
  int two;
  CopyableOnly(int one, int two) : one(one), two(two) {}
  CopyableOnly(const CopyableOnly &) = default;
  CopyableOnly(CopyableOnly &&) = delete;
};

static_assert(std::is_copy_constructible<CopyableOnly>::value, "CopyableOnly must be CopyConstructible");
static_assert(!std::is_move_constructible<CopyableOnly>::value, "CopyableOnly must not be MoveConstructible");

struct NeitherMovableNorCopyable {
  int one;
  int two;
  NeitherMovableNorCopyable(int one, int two) : one(one), two(two) {}
  NeitherMovableNorCopyable(const NeitherMovableNorCopyable &) = delete;
  NeitherMovableNorCopyable(NeitherMovableNorCopyable &&) = delete;
};

static_assert(!std::is_copy_constructible<NeitherMovableNorCopyable>::value, "NeitherMovableNorCopyable must not be CopyConstructible");
static_assert(!std::is_move_constructible<NeitherMovableNorCopyable>::value, "NeitherMovableNorCopyable must not be MovaConstructible");

}  // namespace

BOOST_AUTO_TEST_SUITE(fixed_vector_tests)

BOOST_AUTO_TEST_CASE(check_vector) {
  FixedVector<NeitherMovableNorCopyable> v(3);
  BOOST_CHECK(v.empty());
  BOOST_CHECK_EQUAL(v.size(), 0);
  BOOST_CHECK_EQUAL(v.capacity(), 3);
  BOOST_CHECK_EQUAL(v.remaining(), 3);
}

BOOST_AUTO_TEST_CASE(check_emplace_back) {
  FixedVector<NeitherMovableNorCopyable> v(4);
  auto &x = v.emplace_back(2, 7);
  auto &y = v.emplace_back(5, 3);
  auto &z = v.emplace_back(9, 4);
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
  FixedVector<CopyableOnly> v(2);
  CopyableOnly x(2, 7);
  auto &x2 = v.emplace_back(x);
  BOOST_CHECK(!v.empty());
  BOOST_CHECK_EQUAL(v.size(), 1);
  BOOST_CHECK_EQUAL(v.capacity(), 2);
  BOOST_CHECK_EQUAL(v.remaining(), 1);
  BOOST_CHECK_EQUAL(x2.one, 2);
  BOOST_CHECK_EQUAL(x2.two, 7);
  BOOST_CHECK(&x != &x2);
}

BOOST_AUTO_TEST_CASE(check_push_back_move) {
  FixedVector<MovableOnly> v(2);
  MovableOnly x(2, 7);
  auto &x2 = v.emplace_back(std::move(x));
  BOOST_CHECK(!v.empty());
  BOOST_CHECK_EQUAL(v.size(), 1);
  BOOST_CHECK_EQUAL(v.capacity(), 2);
  BOOST_CHECK_EQUAL(v.remaining(), 1);
  BOOST_CHECK_EQUAL(x2.one, 2);
  BOOST_CHECK_EQUAL(x2.two, 7);
  BOOST_CHECK(&x != &v[0]);
}

BOOST_AUTO_TEST_CASE(check_pop) {
  FixedVector<NeitherMovableNorCopyable> v(2);
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

BOOST_AUTO_TEST_CASE(check_iterator) {
  FixedVector<int> v0(2);
  FixedVector<int> v1(2);
  v0.emplace_back(3);
  v0.emplace_back(7);
  for (auto &e : v0) {
    v1.emplace_back(e);
  }
  BOOST_CHECK(v0 == v1);
}

BOOST_AUTO_TEST_CASE(check_equals) {
  FixedVector<int> v0(2);
  FixedVector<int> v1(3);
  v0.emplace_back(0);
  v0.emplace_back(1);
  v1.emplace_back(0);
  v1.emplace_back(1);
  BOOST_CHECK(v0 == v1);
  BOOST_CHECK(!(v0 != v1));
}

BOOST_AUTO_TEST_CASE(check_not_equals) {
  FixedVector<int> v0(4);
  FixedVector<int> v1(2);
  v0.emplace_back(0);
  v0.emplace_back(1);
  v1.emplace_back(0);
  v1.emplace_back(2);
  BOOST_CHECK(!(v0 == v1));
  BOOST_CHECK(v0 != v1);
}

BOOST_AUTO_TEST_CASE(algorithm_sort) {
  FixedVector<int> v(3);
  v.emplace_back(5);
  v.emplace_back(3);
  v.emplace_back(7);
  std::sort(v.begin(), v.end());
  BOOST_CHECK_EQUAL(v[0], 3);
  BOOST_CHECK_EQUAL(v[1], 5);
  BOOST_CHECK_EQUAL(v[2], 7);
}

BOOST_AUTO_TEST_SUITE_END()
