// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_FIXED_VECTOR_H
#define UNIT_E_FIXED_VECTOR_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include <boost/compressed_pair.hpp>

//! \brief A fixed size but dynamically allocated container.
//!
//! As opposed to an std::vector elements do not not have to be either
//! CopyInsertable or MoveInsertable and insertion will never invalidate
//! references.
//!
//! \tparam T Element type must be EmplaceConstructible and Erasable.
template <typename T, typename Allocator = std::allocator<T>>
class FixedVector {

 private:
  using pointer = T *;
  using allocator_type = std::allocator<T>;
  using allocator_traits = std::allocator_traits<allocator_type>;

  std::size_t m_capacity;
  std::size_t m_size;
  boost::compressed_pair<pointer, allocator_type> m_data_alloc;

  pointer &data() { return m_data_alloc.first(); }
  const pointer &data() const { return m_data_alloc.first(); }
  allocator_type &alloc() { return m_data_alloc.second(); }
  void allocate() { data() = allocator_traits::allocate(alloc(), m_capacity); }
  void deallocate() { allocator_traits::deallocate(alloc(), data(), m_capacity); }

 public:
  using reference = T &;
  using iterator = T *;

  explicit FixedVector(const std::size_t capacity)
      : m_capacity(capacity), m_size(0), m_data_alloc(nullptr) {
    allocate();
  }

  FixedVector() noexcept : m_capacity(0), m_size(0), m_data_alloc(nullptr) {}

  FixedVector(const std::size_t capacity, const allocator_type &allocator)
      : m_capacity(capacity), m_size(0), m_data_alloc(nullptr, allocator) {
    allocate();
  }

  explicit FixedVector(const allocator_type &allocator) noexcept
      : m_capacity(0), m_size(0), m_data_alloc(nullptr, allocator) {}

  T &operator[](std::size_t index) const {
    if (index >= m_size) {
      throw std::runtime_error("index out of bounds");
    }
    return data()[index];
  }

  reference push_back(const T &thing) {
    if (m_size >= m_capacity) {
      throw std::runtime_error("over capacity");
    }
    allocator_traits::construct(alloc(), end(), thing);
    return (*this)[m_size++];
  }

  reference push_back(T &&thing) {
    if (m_size >= m_capacity) {
      throw std::runtime_error("over capacity");
    }
    allocator_traits::construct(alloc(), end(), std::move(thing));
    return (*this)[m_size++];
  }

  template <typename... Args>
  reference emplace_back(Args &&... args) {
    if (m_size >= m_capacity) {
      throw std::runtime_error("over capacity");
    }
    allocator_traits::construct(alloc(), end(), std::forward<Args>(args)...);
    return (*this)[m_size++];
  }

  bool pop() {
    if (m_size > 0) {
      --m_size;
      allocator_traits::destroy(alloc(), end());
      return true;
    }
    return false;
  }

  void clear() {
    while (pop()) {
      // drain
    }
  }

  //! \brief clears contents and invalidates all references
  void resize(const std::size_t capacity) {
    clear();
    deallocate();
    m_capacity = capacity;
    if (capacity == 0) {
      data() = nullptr;
    } else {
      allocate();
    }
  }

  iterator begin() const { return data(); }
  iterator end() const { return begin() + m_size; }
  bool empty() const { return m_size == 0; }
  std::size_t size() const { return m_size; }
  std::size_t capacity() const { return m_capacity; }
  std::size_t remaining() const { return m_capacity - m_size; }

  ~FixedVector() {
    if (data() != nullptr) {
      clear();
      deallocate();
    }
  }
};

#endif  //UNIT_E_FIXED_VECTOR_H
