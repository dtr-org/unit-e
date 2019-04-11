// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_VALIDATION_FLAGS_H
#define UNIT_E_VALIDATION_FLAGS_H

#include <cstdint>

namespace Flags {
template <typename FlagType>
bool IsSet(const FlagType flags, const FlagType flag) {
  return (flags & flag) != 0;
}
}  // namespace Flags

//! Flags for CChainState::ConnectBlock
namespace ConnectBlockFlags {
using Type = std::uint8_t;

//! Default: No Flags.
static constexpr const Type NONE = 0;

//! Bypass expensive checks, used in TestBlockValidity.
static constexpr const Type JUST_CHECK = 1 << 0;

//! \brief Skips the eligibility check in CheckStake.
//!
//! CheckStake is invoked in certain circumstances (like in
//! CBlockTemplate::CreateBlock or certain regtest scenarios)
//! in which there is no eligible coin in a block yet.
static constexpr const Type SKIP_ELIGIBILITY_CHECK = 1 << 2;
}  // namespace ConnectBlockFlags

//! Flags for staking::StakeValidator::CheckStake
namespace CheckStakeFlags {
using Type = std::uint8_t;

//! Default: No Flags.
static constexpr const Type NONE = 0;

//! \brief Skips the eligibility check in CheckStake.
//!
//! CheckStake is invoked in certain circumstances (like in
//! CBlockTemplate::CreateBlock or certain regtest scenarios)
//! in which there is no eligible coin in a block yet.
static constexpr const Type SKIP_ELIGIBILITY_CHECK = 1 << 1;
}  // namespace CheckStakeFlags

//! Flags for TestBlockValidity
namespace TestBlockValidityFlags {
using Type = std::uint8_t;

//! Default: No Flags.
static constexpr const Type NONE = 0;

//! \brief Skips the eligibility check in CheckStake.
//!
//! CheckStake is invoked in certain circumstances (like in
//! CBlockTemplate::CreateBlock or certain regtest scenarios)
//! in which there is no eligible coin in a block yet.
static constexpr const Type SKIP_ELIGIBILITY_CHECK = 1 << 0;

static constexpr const Type SKIP_MERKLE_TREE_CHECK = 1 << 1;
}  // namespace TestBlockValidityFlags

#endif  //UNIT_E_VALIDATION_FLAGS_H
