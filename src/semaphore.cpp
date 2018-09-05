// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "semaphore.h"

#include <util.h>

Semaphore::Semaphore(const size_t initialValue) : m_count{initialValue} {}

void Semaphore::acquire(const size_t amount) {
  LogPrintf("acquire semaphore %p.\n", this);
  std::unique_lock<decltype(m_mutex)> lock{m_mutex};
  m_cv.wait(lock, [this, amount]() { return m_count >= amount; });
  m_count -= amount;
}

void Semaphore::release(const size_t amount) {
  LogPrintf("release semaphore %p.\n", this);
  std::unique_lock<decltype(m_mutex)> lock{m_mutex};
  m_count += amount;
  m_cv.notify_all();
}
