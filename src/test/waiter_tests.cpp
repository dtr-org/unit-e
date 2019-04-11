// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_unite.h>
#include <proposer/waiter.h>
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <cassert>
#include <ctime>
#include <thread>

BOOST_AUTO_TEST_SUITE(waiter_tests)

namespace {

void wait_value(const std::atomic<bool> &value) {
  const auto wait_until = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!value) {
    const auto now = std::chrono::steady_clock::now();
    BOOST_REQUIRE(now < wait_until);
    std::this_thread::yield();
  }
}

}

BOOST_AUTO_TEST_CASE(wait_and_wake_test) {
  proposer::Waiter waiter;

  // Test wait and wake
  {
    std::atomic<bool> result(false);
    std::atomic<bool> started(false);

    std::thread thread([&waiter, &result, &started]() {
      started = true;
      waiter.Wait();
      result = true;
    });

    wait_value(started);
    BOOST_CHECK(started);
    BOOST_CHECK(!result);

    waiter.Wake();

    wait_value(result);
    BOOST_CHECK(started);
    BOOST_CHECK(result);

    thread.join();
  }

  // Test wait up to
  {
    std::atomic<bool> result(false);
    std::atomic<bool> started(false);

    const auto started_at = std::chrono::steady_clock::now();
    std::thread thread([&waiter, &result, &started]() {
      started = true;
      waiter.WaitUpTo(std::chrono::seconds(5));
      result = true;
    });

    wait_value(started);
    BOOST_CHECK(started);
    BOOST_CHECK(!result);

    wait_value(result);
    BOOST_CHECK(started);
    BOOST_CHECK(result);

    const auto finished_at = std::chrono::steady_clock::now();
    BOOST_CHECK(std::chrono::duration_cast<std::chrono::seconds>(finished_at - started_at).count() >= 5);

    thread.join();
  }

  // Test wait + wait up to
  {
    std::atomic<bool> result1(false);
    std::atomic<bool> started1(false);
    std::atomic<bool> result2(false);
    std::atomic<bool> started2(false);

    const auto started_at = std::chrono::steady_clock::now();
    std::thread thread1([&waiter, &result1, &started1]() {
      started1 = true;
      waiter.WaitUpTo(std::chrono::seconds(10));
      result1 = true;
    });
    std::thread thread2([&waiter, &result2, &started2]() {
      started2 = true;
      waiter.Wait();
      result2 = true;
    });

    wait_value(started1);
    wait_value(started2);
    BOOST_CHECK(started1);
    BOOST_CHECK(started2);
    BOOST_CHECK(!result1);
    BOOST_CHECK(!result2);

    waiter.Wake();
    wait_value(result1);
    wait_value(result2);
    BOOST_CHECK(result1);
    BOOST_CHECK(result2);

    const auto finished_at = std::chrono::steady_clock::now();
    BOOST_CHECK(std::chrono::duration_cast<std::chrono::seconds>(finished_at - started_at).count() < 10);

    thread1.join();
    thread2.join();
  }
}

BOOST_AUTO_TEST_SUITE_END()
