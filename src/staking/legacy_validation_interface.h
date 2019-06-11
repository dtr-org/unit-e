// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_STAKING_LEGACY_VALIDATION_INTERFACE_H
#define UNITE_STAKING_LEGACY_VALIDATION_INTERFACE_H

#include <dependency.h>

#include <cstdint>
#include <memory>

class CBlock;
class CBlockHeader;
class CBlockIndex;
class CChainParams;
class CValidationState;

namespace Consensus {
struct Params;
}

namespace staking {

class ActiveChain;
class BlockValidator;
class Network;
class StakeValidator;

//! \brief Interface which is compatible with "old style" checks.
//!
//! Bitcoin validation relies on CheckBlockHeader, CheckBlock,
//! ContextualCheckBlock, and ContextualCheckBlockHeader. The same
//! structure has been kept in unit-e with the addition of CheckStake
//! in StakeValidator.
//!
//! The bitcoin-style functions reference CValidationState whereas
//! the unit-e-style functions are made part of components and carry
//! state of validation through staking::BlockValidationInfo.
//!
//! For backwards compatibility CValidationState has been augmented
//! with staking::BlockValidationInfo. The scope of it is narrower
//! than for CValidationState as we also have BlockValidationResult.
//! To translate into the existing validation interface different
//! implementations are provided for the LegacyValidationInterface:
//! - LegacyValidationInterface::New which accesses the PoS BlockValidator
//! - LegacyValidationInterface::Old which implements the original functions.
class LegacyValidationInterface {

 public:
  virtual bool CheckBlockHeader(
      const CBlockHeader &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params) = 0;

  virtual bool CheckBlock(
      const CBlock &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params,
      bool check_merkle_root) = 0;

  //! Short hand (virtual functions do not go well with default parameters)
  //! CheckBlock(block, validation_state, consensus_params, true, true);
  bool CheckBlock(
      const CBlock &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params) {
    return CheckBlock(block, validation_state, consensus_params, true);
  }

  virtual bool ContextualCheckBlock(
      const CBlock &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params,
      const CBlockIndex *prev_block) = 0;

  virtual bool ContextualCheckBlockHeader(
      const CBlockHeader &block,
      CValidationState &validation_state,
      const CChainParams &chainparams,
      const CBlockIndex *prev_block,
      std::int64_t adjusted_time) = 0;

  virtual ~LegacyValidationInterface() = default;

  //! \brief Instantiates an instance of the old validation functions.
  //!
  //! Although the old functions do not require all these dependencies
  //! they are enumerated here such that New() and LegacyImpl() define
  //! the same interface and can be used interchangeably.
  static std::unique_ptr<LegacyValidationInterface> LegacyImpl(
      Dependency<ActiveChain> active_chain,
      Dependency<BlockValidator> block_validator,
      Dependency<Network> network);

  static std::unique_ptr<LegacyValidationInterface> New(
      Dependency<ActiveChain> active_chain,
      Dependency<BlockValidator> block_validator,
      Dependency<Network> network);

  //! \brief Instantiates an instance of the old validation functions.
  //!
  //! This factory should be used in tests only.
  static std::unique_ptr<LegacyValidationInterface> Old();
};

}  // namespace staking

#endif  // UNITE_STAKING_LEGACY_VALIDATION_INTERFACE_H
