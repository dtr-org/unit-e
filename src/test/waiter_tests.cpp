// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_unite.h>
#include <waiter.h>
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <cassert>
#include <ctime>
#include <thread>

BOOST_AUTO_TEST_SUITE(waiter_tests)

BOOST_AUTO_TEST_CASE(wait_and_wake_test) {
  Waiter waiter;

  std::atomic<bool> result(false);
  std::atomic<bool> started(false);

  std::thread thread([&waiter, &result, &started]() {
    started = true;
    waiter.Wait();
    result = true;
  });

  {
    const time_t startedAt = std::time(nullptr);
    while (!started) {
      // idle waiting - timeout after 30 seconds
      assert(std::time(nullptr) - startedAt < 30);
      std::this_thread::yield();
    }
  }

  BOOST_CHECK(started);
  BOOST_CHECK(!result);

  std::this_thread::yield();

  waiter.Wake();

  {
    const time_t startedAt = std::time(nullptr);
    while (!result) {
      // idle waiting - timeout after 30 seconds
      assert(std::time(nullptr) - startedAt < 30);
      std::this_thread::yield();
    }
  }

  thread.detach();

  BOOST_CHECK(result);
}

BOOST_AUTO_TEST_SUITE_END()
