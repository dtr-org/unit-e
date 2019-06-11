// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_VALIDATION_FLAGS_H
#define UNITE_VALIDATION_FLAGS_H

#include <utiltypetags.h>

#include <cstdint>

namespace Flags {

template <typename FlagType>
bool IsSet(const FlagType flags, const FlagType flag) noexcept {
  return (flags.value & flag.value) != 0;
}

template <typename T>
struct Flag {
  std::uint8_t value;
  explicit Flag(std::uint8_t value) noexcept : value(value) {}
  bool operator==(const T &other) const { return value == other.value; }
  bool operator!=(const T &other) const { return !(*this == other); }
  T    operator& (const T &other) const { return T(value & other.value); }
  T    operator| (const T &other) const { return T(value | other.value); }
  T   &operator|=(const T &other) {
    value |= other.value;
    return static_cast<T&>(*this);
  }
};

}  // namespace Flags


//! Flags for CChainState::ConnectBlock
namespace ConnectBlockFlags {

struct Type : public Flags::Flag<Type> {
  explicit Type(std::uint8_t value) noexcept : Flag(value) {}
};

//! Default: No Flags.
static const Type NONE = Type(0);

//! Bypass expensive checks, used in TestBlockValidity.
static const Type JUST_CHECK = Type(1 << 0);

//! \brief Skips the eligibility check in CheckStake.
//!
//! CheckStake is invoked in certain circumstances (like in
//! CBlockTemplate::CreateBlock or certain regtest scenarios)
//! in which there is no eligible coin in a block yet.
static const Type SKIP_ELIGIBILITY_CHECK = Type(1 << 1);

}  // namespace ConnectBlockFlags


//! Flags for staking::StakeValidator::CheckStake
namespace CheckStakeFlags {

struct Type : public Flags::Flag<Type> {
  explicit Type(std::uint8_t value) noexcept : Flag(value) {}
};

//! Default: No Flags.
static const Type NONE = Type(0);

//! \brief Skips the eligibility check in CheckStake.
//!
//! CheckStake is invoked in certain circumstances (like in
//! CBlockTemplate::CreateBlock or certain regtest scenarios)
//! in which there is no eligible coin in a block yet.
static const Type SKIP_ELIGIBILITY_CHECK = Type(1 << 0);

}  // namespace CheckStakeFlags


//! Flags for TestBlockValidity
namespace TestBlockValidityFlags {

struct Type : public Flags::Flag<Type> {
  explicit Type(std::uint8_t value) noexcept : Flag(value) {}
};

//! Default: No Flags.
static const Type NONE = Type(0);

static const Type SKIP_MERKLE_TREE_CHECK = Type(1 << 0);

//! \brief Skips the eligibility check in CheckStake.
//!
//! CheckStake is invoked in certain circumstances (like in
//! CBlockTemplate::CreateBlock or certain regtest scenarios)
//! in which there is no eligible coin in a block yet.
static const Type SKIP_ELIGIBILITY_CHECK = Type(1 << 1);

}  // namespace TestBlockValidityFlags

#endif  // UNITE_VALIDATION_FLAGS_H
