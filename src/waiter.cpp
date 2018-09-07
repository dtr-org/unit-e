// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <waiter.h>

void Waiter::Wait() {
  std::unique_lock<decltype(m_mutex)> lock(m_mutex);
  m_waiting.test_and_set();
  m_condition.wait(lock, [this]() { return !m_waiting.test_and_set(); });
}

void Waiter::WakeOne() {
  std::unique_lock<decltype(m_mutex)> lock(m_mutex);
  m_waiting.clear();
  m_condition.notify_one();
}

void Waiter::WakeAll() {
  std::unique_lock<decltype(m_mutex)> lock(m_mutex);
  m_waiting.clear();
  m_condition.notify_all();
}
