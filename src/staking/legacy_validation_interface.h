// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_LEGACY_VALIDATION_INTERFACE_H
#define UNIT_E_LEGACY_VALIDATION_INTERFACE_H

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
class StakeValidator;

class LegacyValidationInterface {

 public:
  virtual bool CheckBlockHeader(
      const CBlockHeader &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params,
      bool check_proof_of_work) = 0;

  bool CheckBlockHeader(
      const CBlockHeader &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params) {
    return CheckBlockHeader(block, validation_state, consensus_params, true);
  }

  virtual bool CheckBlock(
      const CBlock &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params,
      bool check_proof_of_work,
      bool check_merkle_root) = 0;

  bool CheckBlock(
      const CBlock &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params,
      bool check_proof_of_work) {
    return CheckBlock(block, validation_state, consensus_params, check_proof_of_work, true);
  }

  bool CheckBlock(
      const CBlock &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params) {
    return CheckBlock(block, validation_state, consensus_params, true, true);
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

  static std::unique_ptr<LegacyValidationInterface> New(
      Dependency<ActiveChain> active_chain,
      Dependency<BlockValidator> block_validator,
      Dependency<StakeValidator> stake_validator);

  static std::unique_ptr<LegacyValidationInterface> LegacyImpl(
      Dependency<ActiveChain> active_chain,
      Dependency<BlockValidator> block_validator,
      Dependency<StakeValidator> stake_validator);

  static std::unique_ptr<LegacyValidationInterface> Old();
};

}  // namespace staking

#endif  //UNIT_E_LEGACY_VALIDATION_INTERFACE_H
