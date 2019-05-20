// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/block_validator.h>

#include <consensus/merkle.h>
#include <consensus/tx_verify.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/script.h>
#include <staking/proof_of_stake.h>
#include <staking/validation_result.h>
#include <streams.h>

#include <limits>

namespace staking {

//! Organization of this BlockValidator implementation:
//!
//! The private part of this class comprises all the business logic of
//! checking blocks. These functions end with the suffix "Internal"
//! and receive a `BlockValidationResult` as last parameter which they will
//! add the violations they find to.
//!
//! Each of these functions assumes that certain checks have already been
//! made. In order to guarantee that there is the whole `BlockValidationInfo`
//! logic of orchestrating which check has already been performed and which
//! was not. These are in the public part at the bottom of this class.
//!
//! This allows reading through the actual validation logic from top to bottom,
//! leaving the details of saving checks/caching status to the brave reader who
//! makes it to the bottom of this file.
class BlockValidatorImpl : public AbstractBlockValidator {

 private:
  Dependency<blockchain::Behavior> m_blockchain_behavior;

  using Error = BlockValidationError;

  //! \brief checks that the coinbase transaction has the right structure, but nothing else
  //!
  //! A well-formed coinbase transaction:
  //! - has at least two inputs
  //! - the first input contains only meta information
  //! - the first input's scriptSig contains the block height and snapshot hash
  BlockValidationResult CheckCoinbaseTransactionInternal(
      const CBlock &block,             //!< [in] The block that contains this coinbase transaction
      const CTransaction &tx,          //!< [in] The transaction to check
      blockchain::Height *height_out,  //!< [out] The height extracted from the scriptSig
      uint256 *snapshot_hash_out       //!< [out] The snapshot hash extracted from the scriptSig
      ) const {
    if (tx.vin.empty()) {
      return BlockValidationResult(Error::NO_META_INPUT);
    } else {
      const BlockValidationResult result = CheckCoinbaseMetaInputInternal(tx.vin[0], height_out, snapshot_hash_out);
      if (!result) {
        return result;
      }
    }
    if (tx.vin.size() < 2) {
      if (!m_blockchain_behavior->IsGenesisBlock(block)) {
        return BlockValidationResult(Error::NO_STAKING_INPUT);
      }
    }
    if (tx.vout.empty()) {
      return BlockValidationResult(Error::COINBASE_TRANSACTION_WITHOUT_OUTPUT);
    }
    return BlockValidationResult::success;
  }

  //! \brief checks that the first input of a coinbase transaction is well-formed
  //!
  //! A well-formed meta input encodes the block height, followed by the snapshot hash.
  //! It is then either terminated by OP_0 or some data follows (forwards-compatible).
  BlockValidationResult CheckCoinbaseMetaInputInternal(
      const CTxIn &in,                 //!< [in] The input to check
      blockchain::Height *height_out,  //!< [out] The height extracted from the scriptSig
      uint256 *snapshot_hash_out       //!< [out] The snapshot hash extracted from the scriptSig
      ) const {

    const CScript &script_sig = in.scriptSig;

    if (!in.prevout.IsNull()) {
      return BlockValidationResult(Error::INVALID_META_INPUT_PREVOUT);
    }

    bool check;
    opcodetype op;
    std::vector<uint8_t> buf;
    CScript::const_iterator it = script_sig.begin();

    // read + check height
    check = script_sig.GetOp(it, op, buf);
    if (!check || (buf.empty() && op != OP_0)) {
      return BlockValidationResult(Error::NO_BLOCK_HEIGHT);
    }
    try {
      CScriptNum height(buf, true);
      if (height < 0 || height > std::numeric_limits<blockchain::Height>::max()) {
        return BlockValidationResult(Error::INVALID_BLOCK_HEIGHT);
      } else if (height_out) {
        *height_out = static_cast<blockchain::Height>(height.getint());
      }
    } catch (scriptnum_error &) {
      return BlockValidationResult(Error::INVALID_BLOCK_HEIGHT);
    }

    // read + check snapshot hash
    check = script_sig.GetOp(it, op, buf);
    if (!check || op != 0x20 || buf.size() != 32) {
      return BlockValidationResult(Error::NO_SNAPSHOT_HASH);
    }
    if (snapshot_hash_out) {
      *snapshot_hash_out = uint256(buf);
    }
    return BlockValidationResult::success;
  }

  //! \brief Checks the blocks signature.
  //!
  //! Every proposer signs a block using the private key which is associated with her
  //! piece of stake, making her eligible to propose that block in the first place.
  //! This ensure that only she can rule on the transactions which are part of the block
  //! (as the contents of the block do not affect proposer eligibility contents could be
  //! altered by any one otherwise).
  //!
  //! This signature is checked here against the public key which is used to unlock the
  //! stake. The piece of information which is signed is the block hash.
  BlockValidationResult CheckBlockSignatureInternal(const CBlock &block) const {
    const uint256 block_hash = block.GetHash();

    const std::vector<CPubKey> keys = staking::ExtractBlockSigningKeys(block);
    if (keys.empty()) {
      return BlockValidationResult(Error::INVALID_BLOCK_PUBLIC_KEY);
    }
    for (const CPubKey &pubkey : keys) {
      if (pubkey.Verify(block_hash, block.signature)) {
        return BlockValidationResult::success;
      }
    }
    // if signature is verified, above loop will return from this function.
    // otherwise we reach here and track the error.
    return BlockValidationResult(Error::BLOCK_SIGNATURE_VERIFICATION_FAILED);
  }

  BlockValidationResult CheckBlockHeaderInternal(
      const CBlockHeader &block_header) const override {

    if (m_blockchain_behavior->CalculateProposingTimestamp(block_header.nTime) != block_header.nTime) {
      return BlockValidationResult(Error::INVALID_BLOCK_TIME);
    }
    return BlockValidationResult::success;
  }

  BlockValidationResult ContextualCheckBlockHeaderInternal(
      const CBlockHeader &block_header,
      const blockchain::Time adjusted_time,
      const CBlockIndex &previous_block) const override {

    if (block_header.hashPrevBlock != *previous_block.phashBlock) {
      return BlockValidationResult(Error::PREVIOUS_BLOCK_DOESNT_MATCH);
    }
    const int64_t block_time = block_header.GetBlockTime();

    if (block_time <= previous_block.GetMedianTimePast()) {
      return BlockValidationResult(Error::BLOCKTIME_TOO_EARLY);
    }
    if (block_time > adjusted_time + m_blockchain_behavior->GetParameters().max_future_block_time_seconds) {
      return BlockValidationResult(Error::BLOCKTIME_TOO_FAR_INTO_FUTURE);
    }
    return BlockValidationResult::success;
  }

  BlockValidationResult CheckBlockInternal(
      const CBlock &block,             //!< [in] The block to check
      blockchain::Height *height_out,  //!< [out] The height extracted from the scriptSig
      uint256 *snapshot_hash_out       //!< [out] The snapshot hash extracted from the scriptSig
      ) const override {

    // check block size limits
    if (!CheckBlockWeight(block)) {
      return BlockValidationResult(Error::INVALID_BLOCK_WEIGHT);
    }

    // check that coinbase transaction is first transaction
    if (block.vtx[0]->GetType() == +TxType::COINBASE) {
      const BlockValidationResult result = CheckCoinbaseTransactionInternal(block, *block.vtx[0], height_out, snapshot_hash_out);
      if (!result) {
        return result;
      }
    } else {
      return BlockValidationResult(Error::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION);
    }

    // check that all other transactions are no coinbase transactions
    for (auto tx = block.vtx.cbegin() + 1; tx != block.vtx.cend(); ++tx) {
      if ((*tx)->GetType() == +TxType::COINBASE) {
        return BlockValidationResult(Error::COINBASE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST);
      }
    }

    if (!CheckSigOpCount(block)) {
      return BlockValidationResult(Error::INVALID_BLOCK_SIGOPS_COUNT);
    }

    // will be set by invocation of BlockMerkleRoot()
    bool duplicate_transactions;

    // check merkle root
    const uint256 expected_merkle_root = BlockMerkleRoot(block, &duplicate_transactions);
    if (block.hashMerkleRoot != expected_merkle_root) {
      return BlockValidationResult(Error::MERKLE_ROOT_MISMATCH);
    }
    if (duplicate_transactions) {
      // UNIT-E TODO: this check is required to mitigate CVE-2012-2459
      // Apparently an alternative construction of the merkle tree avoids this
      // issue completely _and_ results in faster merkle tree construction, see
      // BIP 98 https://github.com/bitcoin/bips/blob/master/bip-0098.mediawiki
      return BlockValidationResult(Error::MERKLE_ROOT_DUPLICATE_TRANSACTIONS);
    }

    // check witness merkle root
    const uint256 expected_witness_merkle_root = BlockWitnessMerkleRoot(block, &duplicate_transactions);
    if (block.hash_witness_merkle_root != expected_witness_merkle_root) {
      return BlockValidationResult(Error::WITNESS_MERKLE_ROOT_MISMATCH);
    }
    if (duplicate_transactions) {
      return BlockValidationResult(Error::WITNESS_MERKLE_ROOT_DUPLICATE_TRANSACTIONS);
    }

    // check finalization merkle tree root
    if (block.hash_finalizer_commits_merkle_root != BlockFinalizerCommitsMerkleRoot(block)) {
      return BlockValidationResult(Error::FINALIZER_COMMITS_MERKLE_ROOT_MISMATCH);
    }

    for (const CTransactionRef &tx : block.vtx) {
      const BlockValidationResult result = CheckTransaction(*tx);
      if (!result) {
        return result;
      }
    }

    // check proposer signature
    if (!m_blockchain_behavior->IsGenesisBlock(block)) {
      // genesis block does not have any stake (as there are no previous blocks)
      const BlockValidationResult result = CheckBlockSignatureInternal(block);
      if (!result) {
        return result;
      }
    }
    return BlockValidationResult::success;
  }

  bool CheckSigOpCount(const CBlock &block) const {
    unsigned int nSigOps = 0;
    for (const auto &tx : block.vtx) {
      nSigOps += GetLegacySigOpCount(*tx);
    }
    const std::uint32_t maximum_sigops_count = m_blockchain_behavior->GetParameters().maximum_sigops_count;
    const std::uint32_t witness_scale_factor = m_blockchain_behavior->GetParameters().witness_scale_factor;
    return nSigOps * witness_scale_factor <= maximum_sigops_count;
  }

  bool CheckBlockWeight(const CBlock &block) const {
    // A block without any transactions is not valid - it must at least have a coinbase.
    if (block.vtx.empty()) {
      return false;
    }
    const std::uint32_t maximum_block_weight = m_blockchain_behavior->GetParameters().maximum_block_weight;
    const std::uint32_t witness_scale_factor = m_blockchain_behavior->GetParameters().witness_scale_factor;
    // Estimate a minimum size of the block such that the more expensive GetSerialSize call can be skipped
    // for blocks which are - under any circumstances - too big.
    const std::size_t number_of_transactions = block.vtx.size();
    const std::size_t lowest_possible_size_of_txns_block = number_of_transactions * m_blockchain_behavior->GetAbsoluteTransactionSizeMinimum();
    const std::size_t lowest_possible_weight_of_txns_block = lowest_possible_size_of_txns_block * m_blockchain_behavior->GetParameters().witness_scale_factor;
    if (lowest_possible_weight_of_txns_block > maximum_block_weight) {
      return false;
    }
    // Check that the block weight matches. The block weight is the size of the block serialized
    // without witness data times the witness scale factor.
    const std::size_t serialized_size = GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS);
    return serialized_size * witness_scale_factor <= maximum_block_weight;
  }

  BlockValidationResult ContextualCheckBlockInternal(
      const CBlock &block,
      const CBlockIndex &prev_block,
      const BlockValidationInfo &validation_info) const override {

    if (validation_info.GetHeight() != static_cast<blockchain::Height>(prev_block.nHeight) + 1) {
      return BlockValidationResult(Error::MISMATCHING_HEIGHT);
    }
    return BlockValidationResult::success;
  }

 public:
  BlockValidationResult CheckCoinbaseTransaction(
      const CBlock &block,
      const CTransaction &coinbase_tx) const override {
    return CheckCoinbaseTransactionInternal(block, coinbase_tx, nullptr, nullptr);
  }

  BlockValidationResult CheckTransaction(
      const CTransaction &tx) const override {
    BlockValidationResult result;
    if (tx.vin.empty()) {
      return BlockValidationResult(Error::INVALID_TRANSACTION_NO_INPUTS);
    }
    if (tx.vout.empty()) {
      return BlockValidationResult(Error::INVALID_TRANSACTION_NO_OUTPUTS);
    }
    const std::size_t size = GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS);
    const std::size_t weight = size * m_blockchain_behavior->GetParameters().witness_scale_factor;
    if (weight > m_blockchain_behavior->GetParameters().maximum_block_weight) {
      return BlockValidationResult(Error::INVALID_TRANSACTION_TOO_BIG);
    }
    std::set<COutPoint> outpoints;
    for (const CTxIn &txin : tx.vin) {
      if (!outpoints.insert(txin.prevout).second) {
        return BlockValidationResult(Error::INVALID_TRANSACTION_DUPLICATE_INPUTS);
      }
      if (txin.prevout.IsNull()) {
        if (tx.IsCoinBase()) {
          continue;
        }
        return BlockValidationResult(Error::INVALID_TRANSACTION_NULL_INPUT);
      }
    }
    switch (tx.GetType()) {
      case TxType::DEPOSIT:
      case TxType::VOTE:
      case TxType::LOGOUT:
        if (!tx.vout[0].scriptPubKey.IsFinalizerCommitScript()) {
          return BlockValidationResult(Error::INVALID_FINALIZER_COMMIT_BAD_SCRIPT);
        }
        break;
      case TxType::REGULAR:
      case TxType::COINBASE:
      case TxType::SLASH:
      case TxType::WITHDRAW:
      case TxType::ADMIN:
        break;
    }
    return result;
  }

  explicit BlockValidatorImpl(Dependency<blockchain::Behavior> blockchain_behavior)
      : m_blockchain_behavior(blockchain_behavior) {}
};

std::unique_ptr<BlockValidator> BlockValidator::New(
    Dependency<blockchain::Behavior> blockchain_behavior) {
  return std::unique_ptr<BlockValidator>(new BlockValidatorImpl(blockchain_behavior));
}

BlockValidationResult AbstractBlockValidator::CheckBlock(
    const CBlock &block,
    BlockValidationInfo *const block_validation_info) const {

  if (block_validation_info && block_validation_info->GetCheckBlockStatus()) {
    // short circuit in case the validation already happened
    return BlockValidationResult::success;
  }
  // Make sure CheckBlockHeader has passed
  if (!block_validation_info || !block_validation_info->GetCheckBlockHeaderStatus()) {
    const BlockValidationResult result = CheckBlockHeader(block, block_validation_info);
    if (!result) {
      return result;
    }
  }
  // perform the actual checks
  blockchain::Height height;
  uint256 snapshot_hash;
  const BlockValidationResult result = CheckBlockInternal(block, &height, &snapshot_hash);
  // save results in block_validation_info if present
  if (block_validation_info) {
    if (result) {
      block_validation_info->MarkCheckBlockSuccessfull(height, snapshot_hash);
    } else {
      block_validation_info->MarkCheckBlockFailed();
    }
  }
  return result;
}

BlockValidationResult AbstractBlockValidator::ContextualCheckBlock(
    const CBlock &block,
    const CBlockIndex &prev_block,
    blockchain::Time adjusted_time,
    BlockValidationInfo *block_validation_info) const {

  // block_validation_info is optional for the caller but carries meta data from
  // the coinbase transaction, hence we make sure to have one available here.
  std::unique_ptr<BlockValidationInfo> ptr = nullptr;
  if (!block_validation_info) {
    ptr.reset(new BlockValidationInfo());
    block_validation_info = ptr.get();
  }
  if (block_validation_info->GetContextualCheckBlockStatus()) {
    // short circuit in case the validation already happened
    return BlockValidationResult::success;
  }
  // Make sure CheckBlock has passed
  if (!block_validation_info->GetCheckBlockStatus()) {
    const BlockValidationResult result = CheckBlock(block, block_validation_info);
    if (!result) {
      return result;
    }
  }
  // Make sure ContextualCheckBlockHeader has passed
  if (!block_validation_info->GetContextualCheckBlockHeaderStatus()) {
    const BlockValidationResult result = ContextualCheckBlockHeader(block, prev_block, adjusted_time, block_validation_info);
    if (!result) {
      return result;
    }
  }
  // perform the actual checks
  const BlockValidationResult result = ContextualCheckBlockInternal(block, prev_block, *block_validation_info);
  // save results in block_validation_info
  if (result) {
    block_validation_info->MarkContextualCheckBlockSuccessfull();
  } else {
    block_validation_info->MarkContextualCheckBlockFailed();
  }
  return result;
}

BlockValidationResult AbstractBlockValidator::CheckBlockHeader(
    const CBlockHeader &block_header,
    BlockValidationInfo *const block_validation_info) const {

  if (block_validation_info && block_validation_info->GetCheckBlockHeaderStatus()) {
    // short circuit in case the validation already happened
    return BlockValidationResult::success;
  }
  // perform the actual checks
  const BlockValidationResult result = CheckBlockHeaderInternal(block_header);
  // save results in block_validation_info if present
  if (block_validation_info) {
    if (result) {
      block_validation_info->MarkCheckBlockHeaderSuccessfull();
    } else {
      block_validation_info->MarkCheckBlockHeaderFailed();
    }
  }
  return result;
}

BlockValidationResult AbstractBlockValidator::ContextualCheckBlockHeader(
    const CBlockHeader &block_header,
    const CBlockIndex &prev_block,
    const blockchain::Time adjusted_time,
    BlockValidationInfo *const block_validation_info) const {

  if (block_validation_info && block_validation_info->GetContextualCheckBlockHeaderStatus()) {
    // short circuit in case the validation already happened
    return BlockValidationResult::success;
  }
  // Make sure CheckBlockHeader has passed
  if (!block_validation_info || !block_validation_info->GetCheckBlockHeaderStatus()) {
    const BlockValidationResult result = CheckBlockHeader(block_header, block_validation_info);
    if (!result) {
      return result;
    }
  }
  // perform the actual checks
  const BlockValidationResult result = ContextualCheckBlockHeaderInternal(block_header, adjusted_time, prev_block);
  // save results in block_validation_info if present
  if (block_validation_info) {
    if (result) {
      block_validation_info->MarkContextualCheckBlockHeaderSuccessfull();
    } else {
      block_validation_info->MarkContextualCheckBlockHeaderFailed();
    }
  }
  return result;
}

}  // namespace staking
