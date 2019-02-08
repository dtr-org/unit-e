// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_UTIL_SCOPE_STOPWATCH_H
#define UNITE_UTIL_SCOPE_STOPWATCH_H

#include <chrono>

#include <util.h>

namespace util {

class ScopeStopwatch {
 public:
  ScopeStopwatch(const ScopeStopwatch &) = delete;

  ScopeStopwatch &operator=(const ScopeStopwatch &) = delete;

  explicit ScopeStopwatch(std::string scope_name)
      : m_start(ClockType::now()),
        m_scope_name(std::move(scope_name)) {
  }

  ~ScopeStopwatch() {
    using namespace std::chrono;
    const auto now = ClockType::now();
    const auto elapsed = duration_cast<microseconds>(now - m_start).count();

    LogPrint(BCLog::BENCH, "\'%s\' took %.2fms\n", m_scope_name, elapsed * 0.001);
  }

 private:
  using ClockType = std::chrono::steady_clock;
  ClockType::time_point m_start;
  std::string m_scope_name;
};

}  // namespace util

#define FUNCTION_STOPWATCH() util::ScopeStopwatch __stopwatch##__func__(__func__)

#define SCOPE_STOPWATCH(scope_name) util::ScopeStopwatch __stopwatch##__func__##__LINE__(scope_name)

#endif  //UNITE_UTIL_SCOPE_STOPWATCH_H
