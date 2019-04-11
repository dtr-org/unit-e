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

namespace ConnectBlockFlags {
  using Type = std::uint8_t;

  static constexpr const Type NONE = 0;
  static constexpr const Type JUST_CHECK = 1 << 0;
  static constexpr const Type ALLOW_SLOW = 1 << 1;
}

namespace CheckStakeFlags {
  using Type = std::uint8_t;

  static constexpr const Type NONE = 0;
  static constexpr const Type ALLOW_SLOW = 1 << 0;
}

#endif //UNIT_E_VALIDATION_FLAGS_H
