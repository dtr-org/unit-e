// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_PARAMETERS_H
#define UNIT_E_BLOCKCHAIN_PARAMETERS_H

#include <amount.h>
#include <blockchain/blockchain_interfaces.h>
#include <blockchain/blockchain_types.h>
#include <consensus/params.h>
#include <primitives/block.h>
#include <protocol.h>
#include <settings.h>
#include <uint256.h>

#include <better-enums/enum.h>
#include <ufp64.h>
#include <cstdint>
#include <type_traits>

namespace blockchain {

struct GenesisBlock {
  CBlock block;
  uint256 hash;

  GenesisBlock() noexcept;
  explicit GenesisBlock(const CBlock &) noexcept;
};

//! \brief The defining parameters of a unit-e blockchain network.
//!
//! This struct is supposed to be a "data class", that is to say, it contains
//! only the values for these parameters, no behavior should be associated
//! with it (i.e. no member functions should be defined for it).
//!
//! For the lack of a better name there is a different, proper class
//! `blockchain::Behavior` that builds a facade on top of these
//! `blockchain::Parameters` (and everything else which might be needed
//! for working with these easily).
struct Parameters {

  //! \brief a function to calculate the block reward.
  //!
  //! The reward function is a pure function that takes as inputs the parameters
  //! that are currently active and the height to propose at.
  using RewardFunction = std::function<CAmount(const Parameters &, Height)>;

  //! \brief a function to calculate the difficulty for a given block.
  //!
  //! The difficulty function is a pure function that takes as inputs the
  //! parameters that are currently active and the height to propose at.
  //! Also it receives a ChainAccess which which allows querying the block
  //! index. It can be used to look at the recent history of blocks and adjust
  //! difficulty accordingly (using whatever metric is provided by the block
  //! index).
  //!
  //! For example the unite difficulty function would be:
  //!
  //! difficultyFunction = [](const Parameters &p, BlockHeight h, ChainAccess &ix) -> Difficulty {
  //!   constexpr int TARGET_TIMESPAN = 14 * 24 * 60 * 60;
  //!   if (h % 2016 != 0) {
  //!     // it it's not difficulty adjust time, just return current difficulty
  //!     return ix.AtDepth(1)->nBits;
  //!   }
  //!   // block at depth 2015 (unite has an off-by-one error here)
  //!   BlockTime nFirstBlockTime = ix.AdDepth(2015)->nTime;
  //!   // Limit adjustment step
  //!   int64_t nActualTimespan = ix.AtDepth(1)->GetBlockTime() - nFirstBlockTime;
  //!   if (nActualTimespan < TARGET_TIMESPAN / 4)
  //!     nActualTimespan = TARGET_TIMESPAN / 4;
  //!   if (nActualTimespan > TARGET_TIMESPAN * 4)
  //!     nActualTimespan = TARGET_TIMESPAN * 4;
  //!
  //!   // Retarget
  //!   const arith_uint256 bnPowLimit = UintToArith256(p.pow_limit);
  //!   arith_uint256 bnNew;
  //!   bnNew.SetCompact(AtDepth(1)->nBits);
  //!   bnNew *= nActualTimespan;
  //!   bnNew /= TARGET_TIMESPAN;
  //!
  //!   if (bnNew > bnPowLimit)
  //!     bnNew = bnPowLimit;
  //!
  //!   return bnNew.GetCompact();
  //! };
  using DifficultyFunction = std::function<Difficulty(const Parameters &, Height, ChainAccess &)>;

  //! \brief a unique identifier for this network.
  //!
  //! The usual predefined identifiers are "main", "test", and "regtest".
  std::string network_name;

  //! \brief The genesis block of this chain.
  GenesisBlock genesis_block;

  //! \brief The usable staking timestamps
  //!
  //! The kernel protocol for Proof of Stake masks timestamps such that a proposer
  //! can use the same stake only every block_stake_timestamp_interval_seconds. That is:
  //! The blocktime used to compute the kernel hash is always:
  //!
  //! kernel_hash_ingredient = current_time - (current_time % block_stake_timestamp_interval_seconds)
  //!
  std::uint32_t block_stake_timestamp_interval_seconds;

  //! \brief frequency of blocks (a block time of 37 secs is one block every 37 secs)
  std::uint32_t block_time_seconds;

  //! \brief maximum time drift that a block is allowed to have with respect to the current time.
  std::uint32_t max_future_block_time_seconds;

  //! \brief Whether nodes in this network should relay non-standard transactions by default or not.
  //!
  //! For ordinary payment transactions there is a notion of "standard", i.e.
  //! the scripts are either standard P2WPKH or P2WSH scripts. Non-standard
  //! transactions that feature fancy script are only relayed if this parameter
  //! is set to true. This parameter can be overriden by a client, it is a
  //! network policy.
  bool relay_non_standard_transactions;

  //! \brief The maximum allowed block size (MAX_BLOCK_SIZE).
  std::uint32_t maximum_block_size;

  //! \brief The maximum allowed weight for a block.
  //!
  //! Unit-e used to have a MAX_BLOCK_SIZE of 1MB which replaced with a new
  //! concept of "block weight" in unite. The block weight is effectively
  //! a block size, but it is computed differently. In the end the "core block"
  //! must still be <= MAX_BLOCK_SIZE but it can carry an additional 3MB of
  //! witness programs (which is the larger part of a block as it contains the
  //! signatures and public keys for unlocking). However this does not make all
  //! blocks 4MB – if theirs a vast asymettry between number of inputs and
  //! number of outputs (i.e. a lot more outputs than inputs) then the effective
  //! block size might not be much bigger than MAX_BLOCK_SIZE.
  std::uint32_t maximum_block_weight;

  //! \brief The maximum allowed size for a serialized block, in bytes.
  //!
  //! This parameter is the size of the complete block, used in networking code.
  //! The "complete block" is the block including magic bytes, block length,
  //! and the block signature (which does not count towards MAX_BLOCK_SIZE).
  std::uint32_t maximum_block_serialized_size;

  //! \brief The maximum allowed number of signature check operations in a block.
  //!
  //! This is a constant which used to be hardcoded in unite and is parameterized
  //! in here. Each opcode is associated with a cost and validity is checked
  //! according to the total cost that it effects (which basically is computing
  //! power required for validation).
  std::uint32_t maximum_block_sigops_cost;

  //! \brief Rewards from proposing blocks can only be spent after the maturity period.
  Height coinbase_maturity;

  //! \brief Stake can only be used after the stake maturity period.
  Height stake_maturity;

  //! \brief The initial amount of premined coins.
  CAmount initial_supply;

  //! \brief The maximum amount of money that can be in the system.
  CAmount maximum_supply;

  //! \brief The base block reward for each period.
  std::vector<CAmount> reward_schedule;

  //! \brief The reward immediately given upon block proposal.
  ufp64::ufp64_t immediate_reward_fraction;

  //! \brief The period size in blocks.
  std::uint32_t period_blocks;

  //! \brief The function calculating the reward for a newly proposed block.
  //!
  //! See description of "RewardFunction". The reward function can (and should)
  //! be given as a pure lambda function.
  RewardFunction reward_function;

  //! \brief The function calculating the difficulty for a block to be newly proposed.
  //!
  //! See description of "DifficultyFunction". The difficulty function can
  //! (and should) be given as a pure lambda function.
  DifficultyFunction difficulty_function;

  //! \brief Whether to allow the "generatetoaddress" and "generate" RPC calls.
  bool mine_blocks_on_demand;

  //! \brief The four magic bytes at the start of P2P messages.
  //!
  //! These are different for different networks and prevent messages from one
  //! network to interfere with messages from the other.
  CMessageHeader::MessageStartChars message_start_characters;

  //! \brief The prefixes for base58 encoded secrets.
  std::vector<unsigned char> base58_prefixes[Base58Type::_size_constant];

  //! \brief A prefix for bech32 encoded strings.
  std::string bech32_human_readable_prefix;

  //! \brief BIP9 deployments information.
  //!
  //! BIP 9 uses the block version bits to carry information about the state
  //! of softforks. The known deployments for this chain are defined in this
  //! parameter.
  //!
  //! See https://github.com/unite/bips/blob/master/bip-0009.mediawiki
  //!
  //! UNIT-E: Use a better-enum for deployments to not resort to the MAX_VERSION_BITS_DEPLOYMENTS hack
  //! (the hack here is to utilize one extra enum for the number of enum values)
  Consensus::BIP9Deployment bip9_deployments[Consensus::MAX_VERSION_BITS_DEPLOYMENTS];

  //! \brief Number of blocks to look at for the signalling of the activation of a soft fork.
  //!
  //! A soft fork is activated if there is a period of length "deploymentConfirmationPeriod"
  //! of which "ruleChangeActivationThreshold" number of blocks signal support
  //! for the soft fork. The confirmation period is a rolling window actually,
  //! that is a soft fork can activate any time the "ruleChangeActivationThreshold"
  //! is met in the last "deploymentConfirmationPeriod" number of blocks.
  std::uint32_t deployment_confirmation_period;

  //! \brief Number of blocks which have to have a softfork activated in a confirmation period.
  std::uint32_t rule_change_activation_threshold;

  //! \brief Suffix of the data dir. In the path "~user/.unit-e/regtest", it's a "regtest" suffix.
  std::string data_dir_suffix;

  //! \brief Default settings to use for this chain.
  Settings default_settings;

  static Parameters Base() noexcept;
  static Parameters TestNet() noexcept;
  static Parameters RegTest() noexcept;
};

}  // namespace blockchain

#endif  //UNIT_E_BLOCKCHAIN_PARAMETERS_H
