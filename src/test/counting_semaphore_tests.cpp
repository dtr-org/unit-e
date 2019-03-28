// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/sync.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <thread>

BOOST_AUTO_TEST_SUITE(semaphore_tests)

BOOST_AUTO_TEST_CASE(barrier_example) {
  proposer::CountingSemaphore semaphore(0);

  std::atomic<bool> t1_started(false);
  std::atomic<bool> t2_started(false);
  std::atomic<bool> t1_after_acquire(false);
  std::atomic<bool> t2_after_acquire(false);

  std::thread t1([&t1_started, &t1_after_acquire, &semaphore]() {
    t1_started = true;
    semaphore.acquire(3);
    t1_after_acquire = true;
  });

  std::thread t2([&t2_started, &t2_after_acquire, &semaphore]() {
    t2_started = true;
    semaphore.acquire(2);
    t2_after_acquire = true;
  });

  {
    const time_t startedAt = std::time(nullptr);
    while (!t1_started && !t2_started) {
      // idle waiting - timeout after 30 seconds
      assert(std::time(nullptr) - startedAt < 30);
      std::this_thread::yield();
    }
  }

  BOOST_CHECK(!t1_after_acquire);
  BOOST_CHECK(!t2_after_acquire);

  semaphore.release(2);

  {
    const time_t startedAt = std::time(nullptr);
    while (!t2_after_acquire) {
      // idle waiting - timeout after 30 seconds
      assert(std::time(nullptr) - startedAt < 30);
      std::this_thread::yield();
    }
  }

  BOOST_CHECK(!t1_after_acquire);
  BOOST_CHECK(t2_after_acquire);

  semaphore.release(2);

  std::this_thread::yield();

  BOOST_CHECK(!t1_after_acquire);
  BOOST_CHECK(t2_after_acquire);

  semaphore.release(2);

  {
    const time_t startedAt = std::time(nullptr);
    while (!t1_after_acquire) {
      // idle waiting - timeout after 30 seconds
      assert(std::time(nullptr) - startedAt < 30);
      std::this_thread::yield();
    }
  }

  BOOST_CHECK(t1_after_acquire);
  BOOST_CHECK(t2_after_acquire);

  t1.detach();
  t2.detach();
}

BOOST_AUTO_TEST_SUITE_END()
