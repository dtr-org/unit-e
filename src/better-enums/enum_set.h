// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_ENUM_SET_H
#define UNIT_E_ENUM_SET_H

#include <array>
#include <type_traits>

namespace {

std::array<std::size_t, 256> ComputeTable() {
  std::array<std::size_t, 256> table{};
  for (std::size_t i = 0; i < 256; ++i) {
    table[i] = 0;
    std::size_t b = i;
    while (b > 0) {
      if (b & 1) {
        ++table[i];
      }
      b >>= 1;
    }
  }
  return table;
}

std::size_t CountBitsInByte(const std::uint8_t byte) {
  static std::array<std::size_t, 256> table = ComputeTable();
  return table[byte];
}

}  // namespace

template <typename T>
std::size_t CountOneBits(T n) {
  static_assert(std::is_integral<T>::value, "T must be an integral type.");
  static_assert(std::is_unsigned<T>::value, "T must be an unsigned type.");
  std::size_t count = 0;
  while (n != 0) {
    count += CountBitsInByte(static_cast<std::uint8_t>(n & 0xff));
    n >>= 8;
  }
  return count;
}

//! \brief a set optimized for holding values of a better-enum type.
//!
//! \tparam Enum A better-enums enum type.
template <typename Enum>
class EnumSet {

 private:
  //! Each bit is for an enum. better-enums can not have more than 64 values.
  std::uint64_t m_bits;

  static constexpr std::uint64_t ONE = 1;

  explicit EnumSet(const std::uint64_t bits) : m_bits(bits) {}

 public:
  EnumSet() : m_bits(0){};
  EnumSet(const EnumSet &) = default;
  EnumSet &operator=(const EnumSet &) = default;

  class iterator {
   private:
    const EnumSet *m_parent;
    std::size_t m_index;

   public:
    iterator(const EnumSet &parent) : m_parent(&parent), m_index(0) {
      while (m_index < 64 && (m_parent->m_bits & (ONE << m_index)) == 0) {
        ++m_index;
      };
    }
    iterator(const EnumSet &parent, const std::size_t index) : m_parent(&parent), m_index(index) {}
    iterator(const iterator &it) = default;
    iterator &operator=(const iterator &it) = default;
    iterator &operator++() {
      do {
        ++m_index;
      } while (m_index < 64 && (m_parent->m_bits & (ONE << m_index)) == 0);
      return *this;
    }
    Enum operator*() const {
      return Enum::_from_index_unchecked(m_index);
    }
    bool operator!=(const iterator &it) const {
      return it.m_index != m_index || it.m_parent != m_parent;
    }
  };
  using const_iterator = iterator;

  iterator begin() const {
    if (IsEmpty()) {
      return end();
    }
    return iterator(*this);
  }

  iterator end() const {
    static iterator endIterator = iterator(*this, 64);
    return endIterator;
  }

  void Add(const Enum value) {
    m_bits |= (ONE << value._value);
  }

  EnumSet &operator+=(const Enum value) {
    Add(value);
    return *this;
  }

  // in a nice language this would be ++=
  EnumSet &operator+=(const EnumSet<Enum> other) {
    *this = *this + other;
    return *this;
  }

  // in a nice language this would be ++
  EnumSet operator+(const EnumSet other) {
    return EnumSet(m_bits | other.m_bits);
  }

  EnumSet operator+(const Enum value) {
    return EnumSet(m_bits) += value;
  }

  bool operator==(const EnumSet &other) {
    return m_bits == other.m_bits;
  }

  bool operator!=(const EnumSet &other) {
    return m_bits != other.m_bits;
  }

  bool operator<(const EnumSet &other) {
    return m_bits < other.m_bits;
  }

  bool Contains(Enum value) const {
    return ((m_bits >> value._value) & ONE) == ONE;
  }

  bool IsEmpty() const {
    return m_bits == 0;
  }

  std::size_t GetSize() const {
    return CountOneBits(m_bits);
  }
};

#endif  //UNIT_E_ENUM_SET_H