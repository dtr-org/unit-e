// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/waiter.h>

namespace proposer {

void Waiter::WaitUpTo(const std::chrono::microseconds duration) {
  std::unique_lock<decltype(m_mutex)> lock(m_mutex);
  m_condition.wait_for(lock, duration);
}

void Waiter::Wait() {
  std::unique_lock<decltype(m_mutex)> lock(m_mutex);
  m_condition.wait(lock);
}

void Waiter::Wake() {
  m_condition.notify_all();
}

}  // namespace proposer
