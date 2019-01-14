// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/block_validator.h>

#include <consensus/merkle.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/script.h>
#include <streams.h>

#include <limits>

namespace staking {

void BlockValidationResult::operator+=(const BlockValidationResult &other) {
  errors += other.errors;
  if (other.height) {
    height = other.height;
  }
  if (other.snapshot_hash) {
    snapshot_hash = other.snapshot_hash;
  }
}

//! \brief Validation succeeded if there are no validation errors
BlockValidationResult::operator bool() const {
  return errors.IsEmpty();
}

std::string BlockValidationResult::ToString() const {
  return errors.ToString();
}

class BlockValidatorImpl : public BlockValidator {

 private:
  Dependency<blockchain::Behavior> m_blockchain_behavior;

  using Error = BlockValidationError;

  //! \brief checks that the coinbase transaction has the right structure, but nothing else
  //!
  //! A well-formed coinbase transaction:
  //! - has at least two inputs
  //! - the first input contains only meta information
  //! - the first input's scriptSig contains the block height and snapshot hash
  BlockValidationResult CheckCoinbaseTransaction(const CTransactionRef &tx) const {
    BlockValidationResult result;

    if (tx->vin.empty()) {
      result.errors += Error::NO_META_INPUT;
    } else {
      result += CheckCoinbaseMetaInput(tx->vin[0]);
    }
    if (tx->vin.size() < 2) {
      result.errors += Error::NO_STAKING_INPUT;
    }
    if (tx->vout.empty()) {
      result.errors += Error::COINBASE_TRANSACTION_WITHOUT_OUTPUT;
    }
    return result;
  }

  //! \brief checks that the first input of a coinbase transaction is well-formed
  //!
  //! A well-formed meta input encodes the block height, followed by the snapshot hash.
  //! It is then either terminated by OP_0 or some data follows (forwards-compatible).
  BlockValidationResult CheckCoinbaseMetaInput(const CTxIn &in) const {
    BlockValidationResult result;

    const CScript &script_sig = in.scriptSig;

    bool check;
    opcodetype op;
    std::vector<uint8_t> buf;
    CScript::const_iterator it = script_sig.begin();

    // read + check height
    check = script_sig.GetOp(it, op, buf);
    if (!check || (buf.empty() && op != OP_0)) {
      result.errors += Error::NO_BLOCK_HEIGHT;
      result.errors += Error::NO_SNAPSHOT_HASH;
      return result;
    }
    try {
      CScriptNum height(buf, true);
      if (height < 0 || height > std::numeric_limits<blockchain::Height>::max()) {
        result.errors += Error::INVALID_BLOCK_HEIGHT;
      } else {
        result.height = static_cast<blockchain::Height>(height.getint());
      }
    } catch (scriptnum_error &) {
      result.errors += Error::INVALID_BLOCK_HEIGHT;
    }

    // read + check snapshot hash
    check = script_sig.GetOp(it, op, buf);
    if (!check || op != 0x20 || buf.size() != 32) {
      result.errors += Error::NO_SNAPSHOT_HASH;
      return result;
    }
    result.snapshot_hash = uint256(buf);

    return result;
  }

  //! \brief checks the proposer signature of the block
  BlockValidationResult CheckBlockSignature(const CBlock &block) const {
    BlockValidationResult result;

    const uint256 block_hash = block.GetHash();

    const auto key = m_blockchain_behavior->ExtractBlockSigningKey(block);
    if (!key) {
      result.errors += Error::INVALID_BLOCK_PUBLIC_KEY;
      return result;
    }
    if (!key->Verify(block_hash, block.signature)) {
      result.errors += Error::BLOCK_SIGNATURE_VERIFICATION_FAILED;
    }
    return result;
  }

 public:
  explicit BlockValidatorImpl(Dependency<blockchain::Behavior> blockchain_behavior)
      : m_blockchain_behavior(blockchain_behavior) {}

  BlockValidationResult ContextualCheckBlockHeader(
      const CBlockHeader &block_header,
      const CBlockIndex &previous_block,
      const blockchain::Time adjusted_time) const override {

    BlockValidationResult result;

    if (block_header.hashPrevBlock != *previous_block.phashBlock) {
      result.errors += Error::PREVIOUS_BLOCK_DOESNT_MATCH;
    }
    if (block_header.GetBlockTime() <= previous_block.GetMedianTimePast()) {
      result.errors += Error::BLOCKTIME_TOO_EARLY;
    }
    if (block_header.GetBlockTime() > adjusted_time + m_blockchain_behavior->GetParameters().max_future_block_time_seconds) {
      result.errors += Error::BLOCKTIME_TOO_FAR_INTO_FUTURE;
    }

    return result;
  }

  BlockValidationResult CheckBlockHeader(const CBlockHeader &block_header) const override {
    BlockValidationResult result;

    if (m_blockchain_behavior->CalculateProposingTimestamp(block_header.nTime) != block_header.nTime) {
      result.errors += Error::INVALID_BLOCK_TIME;
    }

    return result;
  }

  BlockValidationResult ContextualCheckBlock(
      const CBlock &block,
      const CBlockIndex &previous_block) const override {
    BlockValidationResult result;

    // UNIT-E: This is a stub for now.

    return result;
  }

  BlockValidationResult CheckBlock(const CBlock &block) const override {
    BlockValidationResult result;

    // check the block header fields
    result += CheckBlockHeader(block);

    // check that there are transactions
    if (block.vtx.empty()) {
      result.errors += Error::NO_TRANSACTIONS;
      return result;
    }

    // check that coinbase transaction is first transaction
    if (block.vtx[0]->GetType() == +TxType::COINBASE) {
      result += CheckCoinbaseTransaction(block.vtx[0]);
    } else {
      result.errors += Error::FIRST_TRANSACTION_NOT_A_COINBASE_TRANSACTION;
    }

    // check that all other transactions are no coinbase transactions
    for (auto tx = block.vtx.cbegin() + 1; tx != block.vtx.cend(); ++tx) {
      if ((*tx)->GetType() == +TxType::COINBASE) {
        result.errors += Error::COINBASE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST;
      }
    }

    bool duplicate_transactions;

    // check merkle root
    const uint256 expected_merkle_root = BlockMerkleRoot(block, &duplicate_transactions);
    if (block.hashMerkleRoot != expected_merkle_root) {
      result.errors += Error::MERKLE_ROOT_MISMATCH;
    }
    if (duplicate_transactions) {
      // UNIT-E TODO: this check is required to mitigate CVE-2012-2459
      // Apparently an alternative construction of the merkle tree avoids this
      // issue completely _and_ results in faster merkle tree construction, see
      // BIP 98 https://github.com/bitcoin/bips/blob/master/bip-0098.mediawiki
      result.errors += Error::DUPLICATE_TRANSACTIONS_IN_MERKLE_TREE;
    }

    // check witness merkle root
    const uint256 expected_witness_merkle_root = BlockWitnessMerkleRoot(block, &duplicate_transactions);
    if (block.hash_witness_merkle_root != expected_witness_merkle_root) {
      result.errors += Error::WITNESS_MERKLE_ROOT_MISMATCH;
    }
    if (duplicate_transactions) {
      result.errors += Error::DUPLICATE_TRANSACTIONS_IN_WITNESS_MERKLE_TREE;
    }

    // check proposer signature
    result += CheckBlockSignature(block);

    if (!result && m_blockchain_behavior->IsGenesisBlock(block)) {
      // genesis block does not have any stake (as there are no previous blocks)
      result.errors -= Error::NO_STAKING_INPUT;
      // because of this so there's also no public key to sign the block
      result.errors -= Error::INVALID_BLOCK_PUBLIC_KEY;
    }

    return result;
  }

  BlockValidationResult CheckBlockIndex(const CBlockIndex &) const override {
    BlockValidationResult result;
    // UNIT-E: This is an empty stub for now.
    return result;
  }
};

std::unique_ptr<BlockValidator> BlockValidator::New(
    Dependency<blockchain::Behavior> blockchain_behavior) {
  return std::unique_ptr<BlockValidator>(new BlockValidatorImpl(blockchain_behavior));
}

}  // namespace staking
