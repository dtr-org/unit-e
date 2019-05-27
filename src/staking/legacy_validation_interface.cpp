// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/legacy_validation_interface.h>

#include <chain.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <esperanza/checks.h>
#include <primitives/block.h>
#include <staking/active_chain.h>
#include <staking/block_validator.h>
#include <staking/network.h>
#include <staking/stake_validator.h>
#include <uint256.h>
#include <validation.h>

namespace staking {

class LegacyValidationImpl : public LegacyValidationInterface {
 private:
  const Dependency<BlockValidator> m_block_validator;

 public:
  explicit LegacyValidationImpl(const Dependency<BlockValidator> block_validator) : m_block_validator(block_validator) {}

  bool CheckBlockHeader(
      const CBlockHeader &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params) override {
    // This function used to check proof of work only. It will check timestamps in PoS,
    // so it's not superfluous, but with PoW removed it is currently simply returning true.
    return true;
  }

  bool CheckBlock(
      const CBlock &block,
      CValidationState &state,
      const Consensus::Params &consensus_params,
      bool check_merkle_root) override {

    // These are checks that are independent of context.

    if (block.fChecked) {
      return true;
    }

    // Check that the header is valid (particularly PoW).  This is mostly
    // redundant with the call in AcceptBlockHeader.
    if (!CheckBlockHeader(block, state, consensus_params)) {
      return false;
    }

    // Check the merkle root.
    if (check_merkle_root) {
      bool mutated;
      uint256 hashMerkleRoot2 = BlockMerkleRoot(block, &mutated);
      if (block.hashMerkleRoot != hashMerkleRoot2) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txnmrklroot", true, "hashMerkleRoot mismatch");
      }
      // Check for merkle tree malleability (CVE-2012-2459): repeating sequences
      // of transactions in a block without affecting the merkle root of a block,
      // while still invalidating it.
      if (mutated) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-duplicate", true, "duplicate transaction");
      }
      uint256 merkle_root = BlockFinalizerCommitsMerkleRoot(block);
      if (block.hash_finalizer_commits_merkle_root != merkle_root) {
        return state.DoS(100, false, REJECT_INVALID, "bad-finalizercommits-merkleroot", true, "hash_finalizer_commits_merkle_root mismatch");
      }
    }

    // All potential-corruption validation must be done before we do any
    // transaction validation, as otherwise we may mark the header as invalid
    // because we receive the wrong transactions for it.
    // Note that witness malleability is checked in ContextualCheckBlock, so no
    // checks that use witness data may be performed here.

    // Size limits
    if (block.vtx.empty() || block.vtx.size() * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT ||
        ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT) {
      return state.DoS(100, false, REJECT_INVALID, "bad-blk-length", false, "size limits failed");
    }
    // First transaction must be coinbase, the rest must not be
    if (block.vtx.empty() || !block.vtx[0]->IsCoinBase()) {
      return state.DoS(100, false, REJECT_INVALID, "bad-cb-missing", false, "first tx is not coinbase");
    }
    for (std::size_t i = 1; i < block.vtx.size(); i++) {
      if (block.vtx[i]->IsCoinBase()) {
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-multiple", false, "more than one coinbase");
      }
    }
    // Check transactions
    CTransactionRef prevTx;
    for (const auto &tx : block.vtx) {
      if (!CheckTransaction(*tx, state)) {
        return state.Invalid(
            false, state.GetRejectCode(), state.GetRejectReason(),
            strprintf(
                "Transaction check failed (tx hash %s) %s",
                tx->GetHash().ToString(), state.GetDebugMessage()));
      }
      if (prevTx && (tx->GetHash().CompareAsNumber(prevTx->GetHash()) <= 0)) {
        if (tx->GetHash() == prevTx->GetHash()) {
          return state.DoS(
              100, false, REJECT_INVALID, "bad-txns-duplicate", false,
              strprintf(
                  "Duplicate transaction %s", tx->GetHash().ToString()));
        }
        return state.DoS(
            100, false, REJECT_INVALID, "bad-tx-ordering", false,
            strprintf(
                "Transaction order is invalid ((current: %s) < (prev: %s))",
                tx->GetHash().ToString(), prevTx->GetHash().ToString()));
      }
      if (prevTx || !tx->IsCoinBase()) {
        prevTx = tx;
      }
    }
    unsigned int nSigOps = 0;
    for (const auto &tx : block.vtx) {
      nSigOps += GetLegacySigOpCount(*tx);
    }
    if (nSigOps * WITNESS_SCALE_FACTOR > MAX_BLOCK_SIGOPS_COST) {
      return state.DoS(100, false, REJECT_INVALID, "bad-blk-sigops", false, "out-of-bounds SigOpCount");
    }
    if (check_merkle_root) {
      const uint256 hash_witness_merkle_root = BlockWitnessMerkleRoot(block);
      if (block.hash_witness_merkle_root != hash_witness_merkle_root) {
        return state.DoS(100, false, REJECT_INVALID, "bad-witness-merkle-match", true,
                         strprintf("%s: witness merkle commitment mismatch", __func__));
      }
    }
    if (check_merkle_root) {
      block.fChecked = true;
    }
    return true;
  }

  bool ContextualCheckBlock(
      const CBlock &block,
      CValidationState &state,
      const Consensus::Params &consensus_params,
      const CBlockIndex *prev_block) override {

    const int nHeight = prev_block == nullptr ? 0 : prev_block->nHeight + 1;

    // Start enforcing BIP113 (Median Time Past) using versionbits logic.
    int nLockTimeFlags = 0;
    if (VersionBitsState(prev_block, consensus_params, Consensus::DEPLOYMENT_CSV, versionbitscache) == ThresholdState::ACTIVE) {
      nLockTimeFlags |= LOCKTIME_MEDIAN_TIME_PAST;
    }

    int64_t nLockTimeCutoff = (nLockTimeFlags & LOCKTIME_MEDIAN_TIME_PAST)
                                  ? prev_block->GetMedianTimePast()
                                  : block.GetBlockTime();

    // Check that all transactions are finalized
    for (const auto &tx : block.vtx) {
      if (!IsFinalTx(*tx, nHeight, nLockTimeCutoff)) {
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-nonfinal", false, "non-final transaction");
      }
      if (tx->IsFinalizerCommit() && !esperanza::CheckFinalizerCommit(*tx, state)) {
        return false;
      }
    }

    // After the coinbase witness nonce and commitment are verified,
    // we can check if the block weight passes (before we've checked the
    // coinbase witness, it would be possible for the weight to be too
    // large by filling up the coinbase witness, which doesn't change
    // the block hash, so we couldn't mark the block as permanently
    // failed).
    if (GetBlockWeight(block) > MAX_BLOCK_WEIGHT) {
      return state.DoS(100, false, REJECT_INVALID, "bad-blk-weight", false, strprintf("%s : weight limit failed", __func__));
    }

    return true;
  }

  bool ContextualCheckBlockHeader(
      const CBlockHeader &block,
      CValidationState &validation_state,
      const CChainParams &chainparams,
      const CBlockIndex *prev_block,
      std::int64_t adjusted_time) override {
    assert(prev_block != nullptr);

    staking::BlockValidationInfo info;
    // UNIT-E TODO:
    // bitcoin/ContextualCheckBlockHeader does not invoke CheckBlockHeader,
    // but CheckBlockHeader in unit-e checks the timestamp to match with
    // the each-8-seconds-rule. This call is bypassed by marking it successful.
    info.MarkCheckBlockHeaderSuccessfull();
    const staking::BlockValidationResult result =
        m_block_validator->ContextualCheckBlockHeader(block, *prev_block,
                                                      static_cast<blockchain::Time>(adjusted_time), &info);
    return staking::CheckResult(result, validation_state);
  }
};

class NewValidationLogic : public LegacyValidationInterface {
 private:
  const Dependency<ActiveChain> m_active_chain;
  const Dependency<BlockValidator> m_block_validator;
  const Dependency<Network> m_network;

 public:
  NewValidationLogic(
      const Dependency<ActiveChain> active_chain,
      const Dependency<BlockValidator> block_validator,
      const Dependency<Network> network)
      : m_active_chain(active_chain),
        m_block_validator(block_validator),
        m_network(network) {}

  bool CheckBlockHeader(
      const CBlockHeader &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params) override {
    staking::BlockValidationInfo &info = validation_state.GetBlockValidationInfo();
    const staking::BlockValidationResult result = m_block_validator->CheckBlockHeader(block, &info);
    return staking::CheckResult(result, validation_state);
  }

  bool CheckBlock(
      const CBlock &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params,
      bool check_merkle_root) override {
    staking::BlockValidationInfo &info = validation_state.GetBlockValidationInfo();
    const staking::BlockValidationResult result = m_block_validator->CheckBlock(block, &info);
    return staking::CheckResult(result, validation_state);
  }

  bool ContextualCheckBlock(
      const CBlock &block,
      CValidationState &validation_state,
      const Consensus::Params &consensus_params,
      const CBlockIndex *prev_block) override {
    staking::BlockValidationInfo &info = validation_state.GetBlockValidationInfo();
    const auto adjusted_time = static_cast<blockchain::Time>(m_network->GetTime());
    const staking::BlockValidationResult result = m_block_validator->ContextualCheckBlock(block, *prev_block, adjusted_time, &info);
    return staking::CheckResult(result, validation_state);
  }

  bool ContextualCheckBlockHeader(
      const CBlockHeader &block,
      CValidationState &validation_state,
      const CChainParams &chainparams,
      const CBlockIndex *prev_block,
      std::int64_t adjusted_time) override {
    staking::BlockValidationInfo &info = validation_state.GetBlockValidationInfo();
    // UNIT-E TODO:
    // bitcoin/ContextualCheckBlockHeader does not invoke CheckBlockHeader,
    // but CheckBlockHeader in unit-e checks the timestamp to match with
    // the each-8-seconds-rule. This call is bypassed by marking it successful.
    info.MarkCheckBlockHeaderSuccessfull();
    const staking::BlockValidationResult result =
        m_block_validator->ContextualCheckBlockHeader(block, *prev_block,
                                                      static_cast<blockchain::Time>(adjusted_time), &info);
    return staking::CheckResult(result, validation_state);
  }
};

std::unique_ptr<LegacyValidationInterface> LegacyValidationInterface::LegacyImpl(
    const Dependency<ActiveChain> active_chain,
    const Dependency<BlockValidator> block_validator,
    const Dependency<Network> network) {
  return std::unique_ptr<LegacyValidationInterface>(new LegacyValidationImpl(block_validator));
}

std::unique_ptr<LegacyValidationInterface> LegacyValidationInterface::New(
    const Dependency<ActiveChain> active_chain,
    const Dependency<BlockValidator> block_validator,
    const Dependency<Network> network) {
  return std::unique_ptr<LegacyValidationInterface>(new NewValidationLogic(active_chain, block_validator, network));
}

std::unique_ptr<LegacyValidationInterface> LegacyValidationInterface::Old() {
  static auto behavior = blockchain::Behavior::NewForNetwork(blockchain::Network::test);
  static auto validator = BlockValidator::New(behavior.get());
  return LegacyImpl(nullptr, validator.get(), nullptr);
}

}  // namespace staking
