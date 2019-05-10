// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/mocks.h>

namespace mocks {

MethodMockBase::MethodMockBase(Mock *const parent) : m_parent(parent) {
  parent->MockRegister(this);
}

std::uint32_t MethodMockBase::CountInvocations() const {
  return m_invocations;
}

void MethodMockBase::ResetInvocationCounts() {
  m_invocations = 0;
}

void MethodMockBase::CountInteraction() const {
  ++m_invocations;
  m_parent->MockCountInteraction(this);
}

void Mock::MockRegister(MethodMockBase *const method_mock) {
  m_method_mocks.emplace_back(method_mock);
}

void Mock::MockCountInteraction(const MethodMockBase *const method_mock) {
  ++m_interaction_count;
}

std::uint32_t Mock::MockCountInteractions() {
  return m_interaction_count;
}

void Mock::MockReset() {
  for (MethodMockBase *const method_mock : m_method_mocks) {
    method_mock->Reset();
  }
  m_interaction_count = 0;
}

void Mock::MockResetInvocationCounts() {
  for (MethodMockBase *const method_mock : m_method_mocks) {
    method_mock->ResetInvocationCounts();
  }
  m_interaction_count = 0;
}

}  // namespace mocks
