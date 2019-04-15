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

void WaitValue(const std::atomic<bool> &value) {
  const auto wait_until = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (!value) {
    const auto now = std::chrono::steady_clock::now();
    BOOST_REQUIRE(now < wait_until);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

    WaitValue(started);
    BOOST_CHECK(started);
    BOOST_CHECK(!result);

    waiter.Wake();

    WaitValue(result);
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

    WaitValue(started);
    BOOST_CHECK(started);
    BOOST_CHECK(!result);

    WaitValue(result);
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

    WaitValue(started1);
    WaitValue(started2);
    BOOST_CHECK(started1);
    BOOST_CHECK(started2);
    BOOST_CHECK(!result1);
    BOOST_CHECK(!result2);

    waiter.Wake();
    WaitValue(result1);
    WaitValue(result2);
    BOOST_CHECK(result1);
    BOOST_CHECK(result2);

    const auto finished_at = std::chrono::steady_clock::now();
    BOOST_CHECK(std::chrono::duration_cast<std::chrono::seconds>(finished_at - started_at).count() < 10);

    thread1.join();
    thread2.join();
  }
}

BOOST_AUTO_TEST_SUITE_END()
