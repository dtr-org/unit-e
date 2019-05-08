// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_TEST_UTIL_MOCKS_H
#define UNIT_E_TEST_UTIL_MOCKS_H

#include <sync.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace mocks {

class MethodMockBase;

class Mock {
  friend MethodMockBase;

 public:
  void MockReset();
  void MockResetInvocationCounts();
  std::uint32_t MockCountInteractions();

 private:
  void MockRegister(MethodMockBase *method_mock);
  void MockCountInteraction(const MethodMockBase *method_mock);

  std::vector<MethodMockBase *> m_method_mocks;
  mutable std::atomic<std::uint32_t> m_interaction_count{0};
};

class MethodMockBase {
 public:
  std::uint32_t CountInvocations() const;
  virtual void Reset() = 0;
  void ResetInvocationCounts();

 protected:
  explicit MethodMockBase(Mock *parent);
  void CountInteraction() const;
  mutable std::atomic<std::uint32_t> m_invocations{0};
  Mock *const m_parent;
};

template <typename... Args>
class VoidMethodMockImpl : public MethodMockBase {
  using Stub = std::function<void(Args...)>;

 public:
  void SetStub(const Stub &stub) {
    m_stub = stub;
  }
  Stub GetDefaultStub() const {
    return [&](Args...) -> void {};
  }
  void Reset() override {
    SetStub(GetDefaultStub());
    ResetInvocationCounts();
  }
  void operator()(Args... args) const {
    CountInteraction();
    m_stub(args...);
  }
  void forward(Args&&... args) const {
    CountInteraction();
    m_stub(std::forward<Args...>(args)...);
  }

 protected:
  explicit VoidMethodMockImpl(Mock *const parent)
      : MethodMockBase(parent), m_stub(GetDefaultStub()) {}

 private:
  Stub m_stub;
};

template <typename R, typename... Args>
class MethodMockImpl : public MethodMockBase {
  using Stub = std::function<R(Args...)>;

 public:
  using Result = typename std::remove_const<typename std::remove_reference<R>::type>::type;

  void SetResult(Result result) {
    m_result = result;
  }
  void SetStub(const Stub &stub) {
    m_stub = stub;
  }
  Result GetResult() {
    return m_result;
  }
  Stub GetDefaultStub() const {
    return [&](Args...) -> R {
      return m_result;
    };
  }
  void Reset() override {
    SetStub(GetDefaultStub());
    ResetInvocationCounts();
  }
  R operator()(Args... args) const {
    CountInteraction();
    return m_stub(args...);
  }
  R forward(Args&&... args) const {
    CountInteraction();
    return m_stub(std::forward<Args...>(args)...);
  }

 protected:
  explicit MethodMockImpl(Mock *const parent)
      : MethodMockBase(parent), m_stub(GetDefaultStub()) {}
  MethodMockImpl(Mock *const parent, Result default_return_value)
      : MethodMockBase(parent), m_stub(GetDefaultStub()), m_result(default_return_value) {}

 private:
  Stub m_stub;
  Result m_result;
};

class LockMethodMockImpl : public MethodMockBase {
  using Stub = std::function<CCriticalSection &()>;

 public:
  void Reset() override {
    ResetInvocationCounts();
  }
  CCriticalSection &operator()() const {
    CountInteraction();
    return m_cs;
  }

 protected:
  explicit LockMethodMockImpl(Mock *const parent)
      : MethodMockBase(parent) {}

 private:
  mutable CCriticalSection m_cs;
};

template <typename T>
class MethodMock;

//! Mocks typical GetLock() methods
template <typename C>
class MethodMock<CCriticalSection &(C::*)()> : public LockMethodMockImpl {
 public:
  explicit MethodMock(Mock *const parent) : LockMethodMockImpl(parent) {}
};

//! Mocks typical GetLock() const methods
template <typename C>
class MethodMock<CCriticalSection &(C::*)() const> : public LockMethodMockImpl {
 public:
  explicit MethodMock(Mock *const parent) : LockMethodMockImpl(parent) {}
};

//! Mocks const void methods
template <typename C, typename... Args>
class MethodMock<void (C::*)(Args...) const> : public VoidMethodMockImpl<Args...> {
 public:
  explicit MethodMock(Mock *const parent) : VoidMethodMockImpl<Args...>(parent) {}
};

//! Mocks non-const void methods
template <typename C, typename... Args>
class MethodMock<void (C::*)(Args...)> : public VoidMethodMockImpl<Args...> {
 public:
  explicit MethodMock(Mock *const parent) : VoidMethodMockImpl<Args...>(parent) {}
};

//! Mocks const member functions (methods that return something)
template <typename R, typename C, typename... Args>
class MethodMock<R (C::*)(Args...) const> : public MethodMockImpl<R, Args...> {
 public:
  explicit MethodMock(Mock *const parent) : MethodMockImpl<R, Args...>(parent) {}
  MethodMock(Mock *const parent, R result) : MethodMockImpl<R, Args...>(parent, result) {}
};

//! Mocks non-const member functions (methods that return something)
template <typename R, typename C, typename... Args>
class MethodMock<R (C::*)(Args...)> : public MethodMockImpl<R, Args...> {
 public:
  explicit MethodMock(Mock *const parent) : MethodMockImpl<R, Args...>(parent) {}
  MethodMock(Mock *const parent, R result) : MethodMockImpl<R, Args...>(parent, result) {}
};

};  // namespace mocks

#endif  //UNIT_E_TEST_UTIL_MOCKS_H
