// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNIT_E_BLOCKFLAGS_H
#define UNIT_E_BLOCKFLAGS_H

#include <stdint.h>
#include <utiltypetags.h>

#define staking_block_flags_t uint8_t

struct staking_block_flags_tag{};
typedef Newtype<staking_block_flags_tag, staking_block_flags_t, 0u> StakingBlockFlags;

enum class StakingBlockFlag : staking_block_flags_t {
    PROOF_OF_STAKE         = (1u << 0u), //!< marks proof of stake blocks
    FAILED_DUPLICATE_STAKE = (1u << 0u), //!< marks duplicate (failed) blocks
};

inline bool IsStakingBlockFlagSet(StakingBlockFlags flags, StakingBlockFlag flag)
{
    return (static_cast<staking_block_flags_t>(flags) & static_cast<staking_block_flags_t>(flag)) != 0;
}

inline StakingBlockFlags SetStakingBlockFlag(StakingBlockFlags flags, StakingBlockFlag flag)
{
    return StakingBlockFlags(static_cast<staking_block_flags_t>(flags) | static_cast<staking_block_flags_t>(flag));
}


#endif //UNIT_E_BLOCKFLAGS_H
