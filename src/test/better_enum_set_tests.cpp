// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <better-enums/enum.h>
#include <better-enums/enum_set.h>
#include <test/test_unite.h>

#include <boost/test/unit_test.hpp>

#include <set>

BETTER_ENUM(SomeTestEnum, std::uint16_t, A, B, C, D, E, F, G, H)

BOOST_AUTO_TEST_SUITE(better_enum_set_tests)

BOOST_AUTO_TEST_CASE(check_empty) {
  EnumSet<SomeTestEnum> s;
  BOOST_CHECK(s.IsEmpty());
  s += SomeTestEnum::H;
  BOOST_CHECK(!s.IsEmpty());
}

BOOST_AUTO_TEST_CASE(check_size) {
  EnumSet<SomeTestEnum> s;
  BOOST_CHECK_EQUAL(s.GetSize(), 0);

  s += SomeTestEnum::H;
  BOOST_CHECK_EQUAL(s.GetSize(), 1);

  s += SomeTestEnum::C;
  BOOST_CHECK_EQUAL(s.GetSize(), 2);

  s += SomeTestEnum::C;
  BOOST_CHECK_EQUAL(s.GetSize(), 2);
}

BOOST_AUTO_TEST_CASE(check_contains) {
  EnumSet<SomeTestEnum> s;

  s += SomeTestEnum::H;

  BOOST_CHECK(s.Contains(SomeTestEnum::H));
  BOOST_CHECK(!s.Contains(SomeTestEnum::A));
  BOOST_CHECK(!s.Contains(SomeTestEnum::B));
  BOOST_CHECK(!s.Contains(SomeTestEnum::C));
  BOOST_CHECK(!s.Contains(SomeTestEnum::D));
  BOOST_CHECK(!s.Contains(SomeTestEnum::E));
  BOOST_CHECK(!s.Contains(SomeTestEnum::F));
  BOOST_CHECK(!s.Contains(SomeTestEnum::G));

  s += SomeTestEnum::C;

  BOOST_CHECK(s.Contains(SomeTestEnum::H));
  BOOST_CHECK(s.Contains(SomeTestEnum::C));
  BOOST_CHECK(!s.Contains(SomeTestEnum::A));
  BOOST_CHECK(!s.Contains(SomeTestEnum::B));
  BOOST_CHECK(!s.Contains(SomeTestEnum::D));
  BOOST_CHECK(!s.Contains(SomeTestEnum::E));
  BOOST_CHECK(!s.Contains(SomeTestEnum::F));
  BOOST_CHECK(!s.Contains(SomeTestEnum::G));

  s += SomeTestEnum::C;

  BOOST_CHECK(s.Contains(SomeTestEnum::H));
  BOOST_CHECK(s.Contains(SomeTestEnum::C));
  BOOST_CHECK(!s.Contains(SomeTestEnum::A));
  BOOST_CHECK(!s.Contains(SomeTestEnum::B));
  BOOST_CHECK(!s.Contains(SomeTestEnum::D));
  BOOST_CHECK(!s.Contains(SomeTestEnum::E));
  BOOST_CHECK(!s.Contains(SomeTestEnum::F));
  BOOST_CHECK(!s.Contains(SomeTestEnum::G));

}

BOOST_AUTO_TEST_CASE(check_iterator) {
  EnumSet<SomeTestEnum> s;

  s += SomeTestEnum::B;
  s += SomeTestEnum::E;
  s += SomeTestEnum::F;

  std::set<SomeTestEnum> s2;

  for (const auto &e : s) {
    s2.emplace(e);
  }

  std::set<SomeTestEnum> s2Expected;
  s2Expected.emplace(SomeTestEnum::B);
  s2Expected.emplace(SomeTestEnum::E);
  s2Expected.emplace(SomeTestEnum::F);

  BOOST_CHECK_EQUAL(s2.size(), s2Expected.size());
  BOOST_CHECK(s2 == s2Expected);
}

BOOST_AUTO_TEST_SUITE_END()
