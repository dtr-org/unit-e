#ifndef UNIT_E_TRIT_H
#define UNIT_E_TRIT_H

#include <cstdint>

//! \brief A simple datatype that can hold yes/no/maybe kind of information.
class Trit final {

 public:
  static const Trit T_TRUE;
  static const Trit T_FALSE;
  static const Trit T_UNKNOWN;

 private:
  enum value_t : std::uint8_t {
    VALUE_TRUE,
    VALUE_FALSE,
    VALUE_UNKNOWN
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
    return value == VALUE_TRUE;
  }

  //! \brief Whether this is Trit::FALSE
  inline bool IsFalse() const noexcept {
    return value == VALUE_FALSE;
  }

  //! \brief Whether this is Trit::UNKNOWN
  inline bool IsUnknown() const noexcept {
    return value == VALUE_UNKNOWN;
  }

  //! \brief Same as IsTrue() but useful in if statements.
  explicit operator bool() const noexcept {
    return IsTrue();
  }

  //! \brief Ternary AND.
  Trit And(const Trit other) const noexcept {
    if (IsTrue() && other.IsTrue()) {
      return T_TRUE;
    }
    if (IsFalse() || other.IsFalse()) {
      return T_FALSE;
    }
    return T_UNKNOWN;
  }

  //! \brief Ternary OR.
  Trit Or(Trit other) const noexcept {
    if (IsFalse() && other.IsFalse()) {
      return T_FALSE;
    }
    if (IsTrue() || other.IsTrue()) {
      return T_TRUE;
    }
    return T_UNKNOWN;
  }

  //! \brief Ternary NOT.
  Trit Not() const noexcept {
    if (IsTrue()) {
      return T_FALSE;
    }
    if (IsFalse()) {
      return T_TRUE;
    }
    return T_UNKNOWN;
  }

  static Trit And(Trit t1, Trit t2) noexcept {
    return t1.And(t2);
  }

  template <typename... T>
  static Trit And(Trit t, T... ts) noexcept {
    return t.And(And(ts...));
  }

  static Trit Or(Trit t1, Trit t2) noexcept {
    return t1.And(t2);
  }

  template <typename... T>
  static Trit Or(Trit t, T... ts) noexcept {
    return t.And(And(ts...));
  }

};

#endif  //UNIT_E_TRIT_H
