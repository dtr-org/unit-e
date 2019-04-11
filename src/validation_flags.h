// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_VALIDATION_FLAGS_H
#define UNIT_E_VALIDATION_FLAGS_H

#include <cstdint>

namespace Flags {
  template<typename FlagType>
  bool IsSet(const FlagType flags, const FlagType flag) {
    return (flags & flag) != 0;
  }
}

//! Flags for CChainState::ConnectBlock
namespace ConnectBlockFlags {
  using Type = std::uint8_t;

  //! Default: No Flags.
  static constexpr const Type NONE = 0;

  //! Bypass expensive checks, used in TestBlockValidity.
  static constexpr const Type JUST_CHECK = 1 << 0;

  //! CheckStake using CheckStateFlags::ALLOW_SLOW
  static constexpr const Type ALLOW_SLOW = 1 << 1;
}

//! Flags for staking::
namespace CheckStakeFlags {
  using Type = std::uint8_t;

  //! Default: No Flags.
  static constexpr const Type NONE = 0;

  //! Allows disk access (reading a block from disk) to
  //! check a block, instead of just relying on the utxo set.
  //! This flag must not be relied on for consensus critical
  //! behavior.
  static constexpr const Type ALLOW_SLOW = 1 << 0;
}

#endif //UNIT_E_VALIDATION_FLAGS_H
