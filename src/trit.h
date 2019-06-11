// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_TRIT_H
#define UNITE_TRIT_H

#include <cstdint>

//! \brief A simple datatype that can hold yes/no/maybe kind of information.
class Trit final {

 public:
  static const Trit True;
  static const Trit False;
  static const Trit Unknown;

 private:
  enum value_t : std::uint8_t {
    value_true,
    value_false,
    value_unknown
  };

  value_t value;

 public:
  //! \brief Will initialize to either Trit::TRUE or Trit::FALSE
  //!
  //! Useful for converting bool into Trit.
  explicit Trit(bool truth) noexcept;

  //! \brief Will initialize to Trit::UNKNOWN.
  Trit() noexcept;

  //! \brief Whether this is Trit::TRUE
  inline bool IsTrue() const noexcept {
    return value == value_true;
  }

  //! \brief Whether this is Trit::FALSE
  inline bool IsFalse() const noexcept {
    return value == value_false;
  }

  //! \brief Whether this is Trit::UNKNOWN
  inline bool IsUnknown() const noexcept {
    return value == value_unknown;
  }

  //! \brief Same as IsTrue() but useful in if statements.
  explicit operator bool() const noexcept {
    return IsTrue();
  }

  //! \brief Ternary AND.
  Trit And(const Trit other) const noexcept {
    if (IsTrue() && other.IsTrue()) {
      return True;
    }
    if (IsFalse() || other.IsFalse()) {
      return False;
    }
    return Unknown;
  }

  //! \brief Ternary OR.
  Trit Or(Trit other) const noexcept {
    if (IsFalse() && other.IsFalse()) {
      return False;
    }
    if (IsTrue() || other.IsTrue()) {
      return True;
    }
    return Unknown;
  }

  //! \brief Ternary NOT.
  Trit Not() const noexcept {
    if (IsTrue()) {
      return False;
    }
    if (IsFalse()) {
      return True;
    }
    return Unknown;
  }

  static Trit And(Trit t1, Trit t2) noexcept {
    return t1.And(t2);
  }

  template <typename... T>
  static Trit And(Trit t, T... ts) noexcept {
    return t.And(And(ts...));
  }

  static Trit Or(Trit t1, Trit t2) noexcept {
    return t1.Or(t2);
  }

  template <typename... T>
  static Trit Or(Trit t, T... ts) noexcept {
    return t.Or(Or(ts...));
  }

};

#endif  // UNITE_TRIT_H
