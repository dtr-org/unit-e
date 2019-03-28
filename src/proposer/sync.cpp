// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/sync.h>

namespace proposer {

CountingSemaphore::CountingSemaphore(const size_t initialValue)
    : m_count{initialValue} {}

void CountingSemaphore::acquire(const size_t amount) {
  std::unique_lock<decltype(m_mutex)> lock(m_mutex);
  m_cv.wait(lock, [this, amount]() { return m_count >= amount; });
  m_count -= amount;
}

void CountingSemaphore::release(const size_t amount) {
  std::unique_lock<decltype(m_mutex)> lock(m_mutex);
  m_count += amount;
  m_cv.notify_all();
}

}  // namespace proposer
