// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_PARAMETERS_H
#define UNIT_E_BLOCKCHAIN_PARAMETERS_H

#include <blockchain/blockchain_genesis.h>

#include <amount.h>
#include <consensus/params.h>
#include <primitives/block.h>
#include <protocol.h>
#include <uint256.h>

#include <better-enums/enum.h>
#include <cstdint>
#include <type_traits>

namespace blockchain {

using BlockHeight = std::uint32_t;
using MoneySupply = CAmount;

// clang-format off
BETTER_ENUM(
    Network,
    uint8_t,
    main = 0,
    test = 1,
    regtest = 2
)
// clang-format on

struct Parameters {

  //! \brief a function to calculate the block reward.
  //!
  //!
  using RewardFunction = CAmount (*)(const Parameters &, MoneySupply, BlockHeight);

  //! \brief a unique identifier for this network.
  //!
  //!
  const char *networkName;

  //! \brief
  //!
  //!
  const char *genesisBlockHash;

  //! \brief
  //!
  //!
  GenesisBlock const* genesisBlock;

  //! \brief
  //!
  //!
  std::uint32_t blockStakeTimestampIntervalSeconds;

  //! \brief frequency of blocks (a block time of 37 secs is one block every 37 secs)
  //!
  //!
  std::uint32_t blockTimeSeconds;

  //! \brief
  //!
  //!
  bool relayNonStandardTransactions;

  //! \brief The maximum allowed weight for a block.
  //!
  //!
  std::uint32_t maximumBlockWeight;

  //! \brief The maximum allowed size for a serialized block, in bytes.
  //!
  //!
  std::uint32_t maximumBlockSerializedSize;

  //! \brief The maximum allowed number of signature check operations in a block.
  //!
  //!
  std::uint32_t maximumBlockSigopsCost;

  //! \brief Coinstake transaction outputs can only be spent after this number of new blocks.
  //!
  //!
  BlockHeight coinstakeMaturity;

  //! \brief
  //!
  //!
  RewardFunction rewardFunction;

  //! \brief
  //!
  //!
  bool requireStandard;

  //! \brief
  //!
  //!
  bool mineBlocksOnDemand;

  //! \brief
  //!
  //!
  CMessageHeader::MessageStartChars messageStartChars;

  //! \brief
  //!
  //!
  uint32_t pruneAfterHeight;

  //! \brief BIP 9
  //!
  //!
  Consensus::BIP9Deployment vDeployments[Consensus::MAX_VERSION_BITS_DEPLOYMENTS];

  //! \brief BIP 9
  //!
  //!
  std::uint32_t ruleChangeActivationThreshold;

  //! \brief BIP 9
  //!
  //!
  std::uint32_t deploymentConfirmationPeriod;

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
