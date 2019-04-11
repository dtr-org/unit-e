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
  std::mutex m_mutex;
  std::condition_variable m_condition;

 public:
  //! Wait until woken up, but no longer than the given duration.
  void WaitUpTo(std::chrono::microseconds duration);

  //! Wait until woken up.
  void Wait();

  //! Wake all waiting thread.
  void Wake();
};

}  // namespace proposer

#endif  // UNIT_E_PROPOSER_WAITER_H
