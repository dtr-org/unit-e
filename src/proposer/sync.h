// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_SYNC_H
#define UNIT_E_PROPOSER_SYNC_H

#include <stdint.h>
#include <condition_variable>
#include <mutex>

namespace proposer {

//! \brief A simple synchronisation primitive
//!
//! Semaphores are a way for threads to exchange simple signals in a
//! concurrent setting.
//!
//! Example: A cyclic barrier
//!
//! {{{
//!   actor N:
//!     ...do some work...
//!     semaphore.release(1)
//!     semaphore.acquire(N)  -- wait for everybody to finish
//!     ...continue...
//! }}}
//!
//! Example: A count down latch (technically "count up")
//!
//! {{{
//!   actor N:
//!     ...do some work...
//!     semaphore.release(1)
//!     ...finished.
//!   another actor:
//!     semaphore.acquire(N)  -- wait for everybody to finish
//! }}}
//!
//! Example: Wait for a starting shot
//!
//! {{{
//!   actor N:
//!     semaphore.acquire(1)
//!     ...start working...
//!   coordinator:
//!     ...some housekeeping...
//!     semaphore.release(N)  -- fire the guns
//! }}}
//!
//! @see The Little Book of Semaphores by Allen B. Downey
//! http://greenteapress.com/semaphores/LittleBookOfSemaphores.pdf
class CountingSemaphore {
 private:
  size_t m_count;
  std::mutex m_mutex;
  std::condition_variable m_cv;

 public:
  explicit CountingSemaphore(size_t initialValue = 0);

  //! Acquires an amount of N and blocks until it is available.
  void acquire(size_t amount = 1);

  //! Releases an amount of N.
  void release(size_t amount = 1);
};

}  // namespace proposer

#endif  // UNIT_E_SYNC_H
