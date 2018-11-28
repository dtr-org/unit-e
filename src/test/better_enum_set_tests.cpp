// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <better-enums/enum.h>
#include <better-enums/enum_set.h>
#include <test/test_unite.h>

#include <boost/test/unit_test.hpp>

#include <set>

#include <boost/optional.hpp>
#include <chrono>
#include <thread>

namespace space_time {

constexpr std::chrono::seconds operator"" _sec(unsigned long long s) {
  return std::chrono::seconds(s);
}

constexpr std::chrono::milliseconds operator"" _ms(unsigned long long s) {
  return std::chrono::milliseconds(s);
}

BETTER_ENUM(Status, std::uint8_t, FIRST_TRY, STILL_TRYING, DONE_TRYING)

template <typename Clock = std::chrono::steady_clock>
class Salvador {

 private:
  using point_in_time = std::chrono::time_point<Clock>;

  static point_in_time now() {
    return Clock::now();
  }

  template <typename Duration = std::chrono::seconds>
  static Duration diff(point_in_time t0, point_in_time t1) {
    return std::chrono::duration_cast<Duration>(t1 > t0 ? t1 - t0 : t0 - t1);
  }

  boost::optional<point_in_time> first_try = boost::none;

 public:
  Status DoSomething() {
    if (!first_try) {
      first_try = now();
      return Status::FIRST_TRY;
    } else if (diff(now(), first_try.get()) > 5000_ms) {
      return Status::DONE_TRYING;
    } else {
      return Status::STILL_TRYING;
    }
  }
};

}  // namespace space_time

BETTER_ENUM(SomeTestEnum, std::uint16_t, A, B, C, D, E, F, G, H)

BOOST_AUTO_TEST_SUITE(better_enum_set_tests)

BOOST_AUTO_TEST_CASE(point_in_time) {

  using namespace space_time;

  Salvador<> dali;

  Status result = Status::STILL_TRYING;
  while (result != +Status::DONE_TRYING) {
    result = dali.DoSomething();
    std::cout << result << std::endl;
    if (result == +Status::DONE_TRYING) {
      break;
    }
    std::this_thread::sleep_for(1_sec);
  }

}

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

BOOST_AUTO_TEST_CASE(check_union) {
  EnumSet<SomeTestEnum> s1;

  s1 += SomeTestEnum::B;
  s1 += SomeTestEnum::C;
  s1 += SomeTestEnum::F;

  EnumSet<SomeTestEnum> s2;

  s2 += SomeTestEnum::E;
  s2 += SomeTestEnum::F;
  s2 += SomeTestEnum::H;

  EnumSet<SomeTestEnum> s3 = s1 + s2;

  // clang-format off
  BOOST_CHECK(!s3.Contains(SomeTestEnum::A));
  BOOST_CHECK( s3.Contains(SomeTestEnum::B));
  BOOST_CHECK( s3.Contains(SomeTestEnum::C));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::D));
  BOOST_CHECK( s3.Contains(SomeTestEnum::E));
  BOOST_CHECK( s3.Contains(SomeTestEnum::F));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::G));
  BOOST_CHECK( s3.Contains(SomeTestEnum::H));
  // clang-format on
}

BOOST_AUTO_TEST_CASE(check_difference) {
  EnumSet<SomeTestEnum> s1;

  s1 += SomeTestEnum::B;
  s1 += SomeTestEnum::C;
  s1 += SomeTestEnum::F;

  EnumSet<SomeTestEnum> s2;

  s2 += SomeTestEnum::E;
  s2 += SomeTestEnum::F;
  s2 += SomeTestEnum::H;

  EnumSet<SomeTestEnum> s3 = s1 - s2;

  // clang-format off
  BOOST_CHECK(!s3.Contains(SomeTestEnum::A));
  BOOST_CHECK( s3.Contains(SomeTestEnum::B));
  BOOST_CHECK( s3.Contains(SomeTestEnum::C));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::D));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::E));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::F));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::G));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::H));
  // clang-format on
}

BOOST_AUTO_TEST_CASE(check_intersection) {
  EnumSet<SomeTestEnum> s1;

  s1 += SomeTestEnum::B;
  s1 += SomeTestEnum::C;
  s1 += SomeTestEnum::F;

  EnumSet<SomeTestEnum> s2;

  s2 += SomeTestEnum::E;
  s2 += SomeTestEnum::F;
  s2 += SomeTestEnum::H;

  EnumSet<SomeTestEnum> s3 = s1 & s2;

  // clang-format off
  BOOST_CHECK(!s3.Contains(SomeTestEnum::A));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::B));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::C));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::D));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::E));
  BOOST_CHECK( s3.Contains(SomeTestEnum::F));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::G));
  BOOST_CHECK(!s3.Contains(SomeTestEnum::H));
  // clang-format on
}

BOOST_AUTO_TEST_CASE(check_add_set) {
  EnumSet<SomeTestEnum> s1;

  s1 += SomeTestEnum::B;
  s1 += SomeTestEnum::C;
  s1 += SomeTestEnum::F;

  EnumSet<SomeTestEnum> s2;

  s2 += SomeTestEnum::E;
  s2 += SomeTestEnum::F;
  s2 += SomeTestEnum::H;

  s2 += s1;

  // clang-format off
  BOOST_CHECK(!s2.Contains(SomeTestEnum::A));
  BOOST_CHECK( s2.Contains(SomeTestEnum::B));
  BOOST_CHECK( s2.Contains(SomeTestEnum::C));
  BOOST_CHECK(!s2.Contains(SomeTestEnum::D));
  BOOST_CHECK( s2.Contains(SomeTestEnum::E));
  BOOST_CHECK( s2.Contains(SomeTestEnum::F));
  BOOST_CHECK(!s2.Contains(SomeTestEnum::G));
  BOOST_CHECK( s2.Contains(SomeTestEnum::H));
  // clang-format on
}

BOOST_AUTO_TEST_CASE(check_remove) {
  EnumSet<SomeTestEnum> s{SomeTestEnum::A, SomeTestEnum::B, SomeTestEnum::C};

  s -= SomeTestEnum::B;
  s -= SomeTestEnum::D;

  // clang-format off
  BOOST_CHECK( s.Contains(SomeTestEnum::A));
  BOOST_CHECK(!s.Contains(SomeTestEnum::B));
  BOOST_CHECK( s.Contains(SomeTestEnum::C));
  BOOST_CHECK(!s.Contains(SomeTestEnum::D));
  BOOST_CHECK(!s.Contains(SomeTestEnum::E));
  BOOST_CHECK(!s.Contains(SomeTestEnum::F));
  BOOST_CHECK(!s.Contains(SomeTestEnum::G));
  BOOST_CHECK(!s.Contains(SomeTestEnum::H));
  // clang-format on
}

BOOST_AUTO_TEST_CASE(check_contains) {
  EnumSet<SomeTestEnum> s;

  s += SomeTestEnum::H;

  // clang-format off
  BOOST_CHECK(!s.Contains(SomeTestEnum::A));
  BOOST_CHECK(!s.Contains(SomeTestEnum::B));
  BOOST_CHECK(!s.Contains(SomeTestEnum::C));
  BOOST_CHECK(!s.Contains(SomeTestEnum::D));
  BOOST_CHECK(!s.Contains(SomeTestEnum::E));
  BOOST_CHECK(!s.Contains(SomeTestEnum::F));
  BOOST_CHECK(!s.Contains(SomeTestEnum::G));
  BOOST_CHECK( s.Contains(SomeTestEnum::H));
  // clang-format on

  s += SomeTestEnum::C;

  // clang-format off
  BOOST_CHECK(!s.Contains(SomeTestEnum::A));
  BOOST_CHECK(!s.Contains(SomeTestEnum::B));
  BOOST_CHECK( s.Contains(SomeTestEnum::C));
  BOOST_CHECK(!s.Contains(SomeTestEnum::D));
  BOOST_CHECK(!s.Contains(SomeTestEnum::E));
  BOOST_CHECK(!s.Contains(SomeTestEnum::F));
  BOOST_CHECK(!s.Contains(SomeTestEnum::G));
  BOOST_CHECK( s.Contains(SomeTestEnum::H));
  // clang-format on

  s += SomeTestEnum::C;

  // clang-format off
  BOOST_CHECK(!s.Contains(SomeTestEnum::A));
  BOOST_CHECK(!s.Contains(SomeTestEnum::B));
  BOOST_CHECK( s.Contains(SomeTestEnum::C));
  BOOST_CHECK(!s.Contains(SomeTestEnum::D));
  BOOST_CHECK(!s.Contains(SomeTestEnum::E));
  BOOST_CHECK(!s.Contains(SomeTestEnum::F));
  BOOST_CHECK(!s.Contains(SomeTestEnum::G));
  BOOST_CHECK( s.Contains(SomeTestEnum::H));
  // clang-format on
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

BOOST_AUTO_TEST_CASE(check_initializer_list) {
  EnumSet<SomeTestEnum> s1{};

  BOOST_CHECK(s1.IsEmpty());

  EnumSet<SomeTestEnum> s2{SomeTestEnum::C, SomeTestEnum::D, SomeTestEnum::G};

  // clang-format off
  BOOST_CHECK(!s2.Contains(SomeTestEnum::A));
  BOOST_CHECK(!s2.Contains(SomeTestEnum::B));
  BOOST_CHECK( s2.Contains(SomeTestEnum::C));
  BOOST_CHECK( s2.Contains(SomeTestEnum::D));
  BOOST_CHECK(!s2.Contains(SomeTestEnum::E));
  BOOST_CHECK(!s2.Contains(SomeTestEnum::F));
  BOOST_CHECK( s2.Contains(SomeTestEnum::G));
  BOOST_CHECK(!s2.Contains(SomeTestEnum::H));
  // clang-format on
}

BOOST_AUTO_TEST_CASE(empty_iterator) {
  EnumSet<SomeTestEnum> s{};
  std::size_t count = 0;
  for (const auto &e : s) {
    std::ignore = e;
    BOOST_CHECK(++count <= s.GetSize());
  }
}

BOOST_AUTO_TEST_CASE(iterator_with_one_element) {
  EnumSet<SomeTestEnum> s{SomeTestEnum::A};
  std::set<SomeTestEnum> s2;
  std::size_t count = 0;
  for (const auto &e : s) {
    std::ignore = e;
    BOOST_CHECK(++count <= s.GetSize());
  }
}

BOOST_AUTO_TEST_CASE(iterator_checks) {
  EnumSet<SomeTestEnum> s{SomeTestEnum::C, SomeTestEnum::D, SomeTestEnum::G};
  std::set<SomeTestEnum> s2;
  std::size_t count = 0;
  for (const auto &e : s) {
    std::ignore = e;
    BOOST_CHECK(++count <= s.GetSize());
  }
}

BOOST_AUTO_TEST_CASE(iterator_on_set_with_all_elements) {
  EnumSet<SomeTestEnum> s{SomeTestEnum::A, SomeTestEnum::B, SomeTestEnum::C,
                          SomeTestEnum::D, SomeTestEnum::E, SomeTestEnum::F,
                          SomeTestEnum::G, SomeTestEnum::H};
  std::size_t count = 0;
  for (const auto &e : s) {
    std::ignore = e;
    BOOST_CHECK(++count <= s.GetSize());
  }
}

BOOST_AUTO_TEST_SUITE_END()
