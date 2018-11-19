// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_WAITER_H
#define UNIT_E_PROPOSER_WAITER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace proposer {

class Waiter {
 private:
  std::atomic_flag m_waiting = ATOMIC_FLAG_INIT;
  std::mutex m_mutex;
  std::condition_variable m_condition;

 public:
  //! Wait until woken up, but no longer than the given duration.
  template <typename R, typename P>
  void WaitUpTo(std::chrono::duration<R, P> duration) {
    std::unique_lock<decltype(m_mutex)> lock(m_mutex);
    m_waiting.test_and_set();
    m_condition.wait_for(lock, duration,
                         [this]() { return !m_waiting.test_and_set(); });
  }

  //! Wait until woken up.
  void Wait();

  //! Wake all waiting thread.
  void Wake();
};

}  // namespace proposer

#endif  // UNIT_E_PROPOSER_WAITER_H
