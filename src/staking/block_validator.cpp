// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/block_validator.h>

#include <consensus/merkle.h>
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
  void CheckCoinbaseTransactionInternal(
      const CTransaction &tx,          //!< [in] The transaction to check
      blockchain::Height *height_out,  //!< [out] The height extracted from the scriptSig
      uint256 *snapshot_hash_out,      //!< [out] The snapshot hash extracted from the scriptSig
      BlockValidationResult &result    //!< [in,out] The validation result
      ) const {
    if (tx.vin.empty()) {
      result.errors += Error::NO_META_INPUT;
    } else {
      CheckCoinbaseMetaInputInternal(tx.vin[0], height_out, snapshot_hash_out, result);
    }
    if (tx.vin.size() < 2) {
      result.AddError(Error::NO_STAKING_INPUT);
    }
    if (tx.vout.empty()) {
      result.AddError(Error::COINBASE_TRANSACTION_WITHOUT_OUTPUT);
    }
  }

  //! \brief checks that the first input of a coinbase transaction is well-formed
  //!
  //! A well-formed meta input encodes the block height, followed by the snapshot hash.
  //! It is then either terminated by OP_0 or some data follows (forwards-compatible).
  void CheckCoinbaseMetaInputInternal(
      const CTxIn &in,                 //!< [in] The input to check
      blockchain::Height *height_out,  //!< [out] The height extracted from the scriptSig
      uint256 *snapshot_hash_out,      //!< [out] The snapshot hash extracted from the scriptSig
      BlockValidationResult &result    //!< [in,out] The validation result
      ) const {

    const CScript &script_sig = in.scriptSig;

    bool check;
    opcodetype op;
    std::vector<uint8_t> buf;
    CScript::const_iterator it = script_sig.begin();

    // read + check height
    check = script_sig.GetOp(it, op, buf);
    if (!check || (buf.empty() && op != OP_0)) {
      result.AddError(Error::NO_BLOCK_HEIGHT);
      result.AddError(Error::NO_SNAPSHOT_HASH);
      return;
    }
    try {
      CScriptNum height(buf, true);
      if (height < 0 || height > std::numeric_limits<blockchain::Height>::max()) {
        result.AddError(Error::INVALID_BLOCK_HEIGHT);
      } else if (height_out) {
        *height_out = static_cast<blockchain::Height>(height.getint());
      }
    } catch (scriptnum_error &) {
      result.AddError(Error::INVALID_BLOCK_HEIGHT);
    }

    // read + check snapshot hash
    check = script_sig.GetOp(it, op, buf);
    if (!check || op != 0x20 || buf.size() != 32) {
      result.AddError(Error::NO_SNAPSHOT_HASH);
      return;
    }
    if (snapshot_hash_out) {
      *snapshot_hash_out = uint256(buf);
    }
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
  void CheckBlockSignatureInternal(const CBlock &block, BlockValidationResult &result) const {
    const uint256 block_hash = block.GetHash();

    const std::vector<CPubKey> keys = staking::ExtractBlockSigningKeys(block);
    if (keys.empty()) {
      result.AddError(Error::INVALID_BLOCK_PUBLIC_KEY);
      return;
    }
    for (const CPubKey &pubkey : keys) {
      if (pubkey.Verify(block_hash, block.signature)) {
        return;
      }
    }
    // if signature is verified, above loop will return from this function.
    // otherwise we reach here and track the error.
    result.AddError(Error::BLOCK_SIGNATURE_VERIFICATION_FAILED);
  }

  void CheckBlockHeaderInternal(
      const CBlockHeader &block_header,
      BlockValidationResult &result) const override {

    if (m_blockchain_behavior->CalculateProposingTimestamp(block_header.nTime) != block_header.nTime) {
      result.AddError(Error::INVALID_BLOCK_TIME);
    }
  }

  void ContextualCheckBlockHeaderInternal(
      const CBlockHeader &block_header,
      const blockchain::Time adjusted_time,
      const CBlockIndex &previous_block,
      BlockValidationResult &result) const override {

    if (block_header.hashPrevBlock != *previous_block.phashBlock) {
      result.AddError(Error::PREVIOUS_BLOCK_DOESNT_MATCH);
    }

    const int64_t block_time = block_header.GetBlockTime();

    if (block_time < previous_block.GetMedianTimePast()) {
      result.AddError(Error::BLOCKTIME_TOO_EARLY);
    }

    const bool on_demand = m_blockchain_behavior->GetParameters().mine_blocks_on_demand;
    if (!on_demand && block_time == previous_block.GetMedianTimePast()) {
      result.AddError(Error::BLOCKTIME_TOO_EARLY);
    }

    if (block_time > adjusted_time + m_blockchain_behavior->GetParameters().max_future_block_time_seconds) {
      result.AddError(Error::BLOCKTIME_TOO_FAR_INTO_FUTURE);
    }
  }

  void CheckBlockInternal(
      const CBlock &block,             //!< [in] The block to check
      blockchain::Height *height_out,  //!< [out] The height extracted from the scriptSig
      uint256 *snapshot_hash_out,      //!< [out] The snapshot hash extracted from the scriptSig
      BlockValidationResult &result    //!< [in,out] The validation result
      ) const override {

    // check that there are transactions
    if (block.vtx.empty()) {
      result.AddError(Error::NO_TRANSACTIONS);
      return;
    }

    // check that coinbase transaction is first transaction
    if (block.vtx[0]->GetType() == +TxType::COINBASE) {
      CheckCoinbaseTransactionInternal(*block.vtx[0], height_out, snapshot_hash_out, result);
    } else {
      result.AddError(Error::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION);
    }

    // check that all other transactions are no coinbase transactions
    for (auto tx = block.vtx.cbegin() + 1; tx != block.vtx.cend(); ++tx) {
      if ((*tx)->GetType() == +TxType::COINBASE) {
        result.AddError(Error::COINBASE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST);
      }
    }

    // will be set by invocation of BlockMerkleRoot()
    bool duplicate_transactions;

    // check merkle root
    const uint256 expected_merkle_root = BlockMerkleRoot(block, &duplicate_transactions);
    if (block.hashMerkleRoot != expected_merkle_root) {
      result.AddError(Error::MERKLE_ROOT_MISMATCH);
    }
    if (duplicate_transactions) {
      // UNIT-E TODO: this check is required to mitigate CVE-2012-2459
      // Apparently an alternative construction of the merkle tree avoids this
      // issue completely _and_ results in faster merkle tree construction, see
      // BIP 98 https://github.com/bitcoin/bips/blob/master/bip-0098.mediawiki
      result.AddError(Error::MERKLE_ROOT_DUPLICATE_TRANSACTIONS);
    }

    // check witness merkle root
    const uint256 expected_witness_merkle_root = BlockWitnessMerkleRoot(block, &duplicate_transactions);
    if (block.hash_witness_merkle_root != expected_witness_merkle_root) {
      result.AddError(Error::WITNESS_MERKLE_ROOT_MISMATCH);
    }
    if (duplicate_transactions) {
      result.AddError(Error::WITNESS_MERKLE_ROOT_DUPLICATE_TRANSACTIONS);
    }

    if (block.hash_finalizer_commits_merkle_root != BlockFinalizerCommitsMerkleRoot(block)) {
      result.AddError(Error::FINALIZER_COMMITS_MERKLE_ROOT_MISMATCH);
    }

    // check proposer signature
    CheckBlockSignatureInternal(block, result);

    if (!result && m_blockchain_behavior->IsGenesisBlock(block)) {
      // genesis block does not have any stake (as there are no previous blocks)
      result.RemoveError(Error::NO_STAKING_INPUT);
      // because of this so there's also no public key to sign the block
      result.RemoveError(Error::INVALID_BLOCK_PUBLIC_KEY);
    }
  }

  void ContextualCheckBlockInternal(
      const CBlock &block,
      const CBlockIndex &prev_block,
      const BlockValidationInfo &validation_info,
      BlockValidationResult &result) const override {

    if (validation_info.GetHeight() != static_cast<blockchain::Height>(prev_block.nHeight) + 1) {
      result.AddError(Error::MISMATCHING_HEIGHT);
    }
  }

 public:
  BlockValidationResult CheckCoinbaseTransaction(
      const CTransaction &coinbase_tx) const override {
    BlockValidationResult result;
    CheckCoinbaseTransactionInternal(coinbase_tx, nullptr, nullptr, result);
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

  BlockValidationResult result;
  if (block_validation_info && block_validation_info->GetCheckBlockStatus()) {
    // short circuit in case the validation already happened
    return result;
  }
  // Make sure CheckBlockHeader has passed
  if (!block_validation_info || !block_validation_info->GetCheckBlockHeaderStatus()) {
    result.AddAll(CheckBlockHeader(block, block_validation_info));
    if (!result) {
      return result;
    }
  }
  // perform the actual checks
  blockchain::Height height;
  uint256 snapshot_hash;
  CheckBlockInternal(block, &height, &snapshot_hash, result);
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

  BlockValidationResult result;
  // block_validation_info is optional for the caller but carries meta data from
  // the coinbase transaction, hence we make sure to have one available here.
  std::unique_ptr<BlockValidationInfo> ptr = nullptr;
  if (!block_validation_info) {
    ptr.reset(new BlockValidationInfo());
    block_validation_info = ptr.get();
  }
  if (block_validation_info->GetContextualCheckBlockStatus()) {
    // short circuit in case the validation already happened
    return result;
  }
  // Make sure CheckBlock has passed
  if (!block_validation_info->GetCheckBlockStatus()) {
    result.AddAll(CheckBlock(block, block_validation_info));
    if (!result) {
      return result;
    }
  }
  // Make sure ContextualCheckBlockHeader has passed
  if (!block_validation_info->GetContextualCheckBlockHeaderStatus()) {
    result.AddAll(ContextualCheckBlockHeader(block, prev_block, adjusted_time, block_validation_info));
    if (!result) {
      return result;
    }
  }
  // perform the actual checks
  ContextualCheckBlockInternal(block, prev_block, *block_validation_info, result);
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

  BlockValidationResult result;
  if (block_validation_info && block_validation_info->GetCheckBlockHeaderStatus()) {
    // short circuit in case the validation already happened
    return result;
  }
  // perform the actual checks
  CheckBlockHeaderInternal(block_header, result);
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

  BlockValidationResult result;
  if (block_validation_info && block_validation_info->GetContextualCheckBlockHeaderStatus()) {
    // short circuit in case the validation already happened
    return result;
  }
  // Make sure CheckBlockHeader has passed
  if (!block_validation_info || !block_validation_info->GetCheckBlockHeaderStatus()) {
    result.AddAll(CheckBlockHeader(block_header, block_validation_info));
    if (!result) {
      return result;
    }
  }
  // perform the actual checks
  ContextualCheckBlockHeaderInternal(block_header, adjusted_time, prev_block, result);
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
