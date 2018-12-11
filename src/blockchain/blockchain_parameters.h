// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_PARAMETERS_H
#define UNIT_E_BLOCKCHAIN_PARAMETERS_H

#include <blockchain/blockchain_genesis.h>

#include <amount.h>
#include <blockchain/blockchain_interfaces.h>
#include <blockchain/blockchain_types.h>
#include <consensus/params.h>
#include <primitives/block.h>
#include <protocol.h>
#include <uint256.h>

#include <better-enums/enum.h>
#include <cstdint>
#include <type_traits>

namespace blockchain {

struct Parameters {

  //! \brief a function to calculate the block reward.
  //!
  //! The reward function is a pure function that takes as inputs the parameters
  //! that are currently active, the total amount of money in the system at the
  //! height to propose a block for, and the height to propose at. This allows
  //! for modelling all kinds of reward functions with or without inflation,
  //! fixed block reward, etc.
  //!
  //! The bitcoin reward function for instance would be:
  //!
  //! rewardFunction = [](const Parameters &p, MoneySupply s, BlockHeight h) -> CAmount {
  //!   constexpr CAmount initial_reward = 50 * UNIT;
  //!   int halvings = h / 210000;
  //!   if (halvings >= 64) {
  //!     return 0;
  //!   }
  //!   return initial_reward >> halvings;
  //! };
  //!
  //! A reward function that yields approximately two percent inflation would be:
  //!
  //! rewardFunction = [](const Parameters &p, MoneySupply s, BlockHeight h) -> CAmount {
  //!   constexpr uint64_t secondsInAYear = 365 * 24 * 60 * 60;
  //!   // 2 percent inflation (2% of current money supply distributed over all blocks in a year)
  //!   return (s * 2 / 100) / (secondsInAYear / p.blockStakeTimestampIntervalSeconds);
  //! };
  using RewardFunction = CAmount (*)(const Parameters &, MoneySupply, BlockHeight);

  //! \brief a function to calculate the difficulty for a given block.
  //!
  //! The difficulty function is a pure function that takes as inputs the
  //! parameters that are currently active and the height to propose at.
  //! Also it receives a DepthIterator which points at the current tip of the
  //! chain. It can be used to look at the recent history of blocks and adjust
  //! difficulty accordingly (using whatever metric is provided by the block
  //! index).
  //!
  //! For example the bitcoin difficulty function would be:
  //!
  //! difficultyFunction = [](const Parameters &p, BlockHeight h, ChainAccess &ix) -> Difficulty {
  //!   constexpr int TARGET_TIMESPAN = 14 * 24 * 60 * 60;
  //!   if (h % 2016 != 0) {
  //!     // it it's not difficulty adjust time, just return current difficulty
  //!     return ix->nBits;
  //!   }
  //!   // block at depth 2015 (ix points at the block before the one we're proposing)
  //!   BlockTime nFirstBlockTime = ix[2015].nTime;
  //!   // Limit adjustment step
  //!   int64_t nActualTimespan = ix->GetBlockTime() - nFirstBlockTime;
  //!   if (nActualTimespan < TARGET_TIMESPAN / 4)
  //!     nActualTimespan = TARGET_TIMESPAN / 4;
  //!   if (nActualTimespan > TARGET_TIMESPAN * 4)
  //!     nActualTimespan = TARGET_TIMESPAN * 4;
  //!
  //!   // Retarget
  //!   const arith_uint256 bnPowLimit = UintToArith256(p.pow_limit);
  //!   arith_uint256 bnNew;
  //!   bnNew.SetCompact(ix->nBits);
  //!   bnNew *= nActualTimespan;
  //!   bnNew /= TARGET_TIMESPAN;
  //!
  //!   if (bnNew > bnPowLimit)
  //!     bnNew = bnPowLimit;
  //!
  //!   return bnNew.GetCompact();
  //! };
  using DifficultyFunction = Difficulty (*)(const Parameters &, BlockHeight, ChainAccess &);

  //! \brief a unique identifier for this network.
  //!
  //! The usual predefined identifiers are "main", "test", and "regtest".
  const char *networkName;

  //! \brief The genesis block of this chain.
  GenesisBlock const *genesisBlock;

  //! \brief The usable staking timestamps
  //!
  //! The kernel protocol for Proof of Stake masks timestamps such that a proposer
  //! can use the same stake only every blockStakeTimestampIntervalSeconds. That is:
  //! The blocktime used to compute the kernel hash is always:
  //!
  //! kernel_hash_ingredient = current_time - (current_time % blockStakeTimestampIntervalSeconds)
  //!
  std::uint32_t blockStakeTimestampIntervalSeconds;

  //! \brief frequency of blocks (a block time of 37 secs is one block every 37 secs)
  std::uint32_t blockTimeSeconds;

  //! \brief Whether nodes in this network should relay non-standard transactions by default or not.
  //!
  //! For ordinary payment transactions there is a notion of "standard", i.e.
  //! the scripts are either standard P2WPKH or P2WSH scripts. Non-standard
  //! transactions that feature fancy script are only relayed if this parameter
  //! is set to true. This parameter can be overriden by a client, it is a
  //! network policy.
  bool relayNonStandardTransactions;

  //! \brief The maximum allowed block size (MAX_BLOCK_SIZE).
  std::uint32_t maximumBlockSize;

  //! \brief The maximum allowed weight for a block.
  //!
  //! Bitcoin used to have a MAX_BLOCK_SIZE of 1MB which replaced with a new
  //! concept of "block weight" in bitcoin. The block weight is effectively
  //! a block size, but it is computed differently. In the end the "core block"
  //! must still be <= MAX_BLOCK_SIZE but it can carry an additional 3MB of
  //! witness programs (which is the larger part of a block as it contains the
  //! signatures and public keys for unlocking). However this does not make all
  //! blocks 4MB – if theirs a vast asymettry between number of inputs and
  //! number of outputs (i.e. a lot more outputs than inputs) then the effective
  //! block size might not be much bigger than MAX_BLOCK_SIZE.
  std::uint32_t maximumBlockWeight;

  //! \brief The maximum allowed size for a serialized block, in bytes.
  //!
  //! This parameter is the size of the complete block, used in networking code.
  //! The "complete block" is the block including magic bytes, block length,
  //! and the block signature (which does not count towards MAX_BLOCK_SIZE).
  std::uint32_t maximumBlockSerializedSize;

  //! \brief The maximum allowed number of signature check operations in a block.
  //!
  //! This is a constant which used to be hardcoded in bitcoin and is parameterized
  //! in here. Each opcode is associated with a cost and validity is checked
  //! according to the total cost that it effects (which basically is computing
  //! power required for validation).
  std::uint32_t maximumBlockSigopsCost;

  //! \brief Coinstake transaction outputs can only be used for staking at this depth.
  BlockHeight coinstakeMaturity;

  //! \brief The function calculating the reward for a newly proposed block.
  //!
  //! See description of "RewardFunction". The reward function can (and should)
  //! be given as a pure lambda function.
  RewardFunction rewardFunction;

  //! \brief The function calculating the difficulty for a block to be newly proposed.
  //!
  //! See description of "DifficultyFunction". The difficulty function can
  //! (and should) be given as a pure lambda function.
  DifficultyFunction difficultyFunction;

  //! \brief Whether to allow the "generatetoaddress" and "generate" RPC calls.
  bool mineBlocksOnDemand;

  //! \brief The four magic bytes at the start of P2P messages.
  //!
  //! These are different for different networks and prevent messages form one
  //! network to interfere with messages from the other.
  CMessageHeader::MessageStartChars messageStartChars;

  //! \brief BIP9 deployments information.
  //!
  //! BIP 9 used the block version bits to carry information about the state
  //! of softforks. The known deployments for this chain are defined in this
  //! parameter.
  //!
  //! See https://github.com/bitcoin/bips/blob/master/bip-0009.mediawiki
  //!
  //! UNIT-E: Use a better-enum for deployments to not resort to the MAX_VERSION_BITS_DEPLOYMENTS hack
  //! (the hack here is to utilize one extra enum for the number of enum values)
  Consensus::BIP9Deployment vDeployments[Consensus::MAX_VERSION_BITS_DEPLOYMENTS];

  //! \brief Number of blocks to look at for the signaling of the activation of a soft fork.
  //!
  //! A soft fork is activated if there is a period of length "deploymentConfirmationPeriod"
  //! of which "ruleChangeActivationThreshold" number of blocks signal support
  //! for the soft fork. The confirmation period is a rolling window actually,
  //! that is a soft fork can activate any time the "ruleChangeActivationThreshold"
  //! is met in the last "deploymentConfirmationPeriod" number of blocks.
  std::uint32_t deploymentConfirmationPeriod;

  //! \brief Number of blocks which have to have a softfork activated in a confirmation period.
  std::uint32_t ruleChangeActivationThreshold;

  static const Parameters &MainNet();
  static const Parameters &TestNet();
  static const Parameters &RegTest();
};

static_assert(std::is_trivial<Parameters>::value,
              "blockchain::Parameters is expected to be a plain old data type.");
static_assert(std::is_standard_layout<Parameters>::value,
              "blockchain::Parameters is expected to be a plain old data type.");

}  // namespace blockchain

#endif  //UNIT_E_BLOCKCHAIN_PARAMETERS_H
