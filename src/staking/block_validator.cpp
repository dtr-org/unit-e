// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/block_validator.h>

#include <consensus/merkle.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/script.h>
#include <streams.h>

namespace staking {

class BlockValidatorImpl : public BlockValidator {

 private:
  using Error = BlockValidationError;

  //! \brief checks that the coinstake transaction has the right structure, but nothing else
  //!
  //! A well-formed coinstake transaction:
  //! - has at least two inputs
  //! - the first input contains only meta information
  //! - the first input's scriptSig contains the block height and snapshot hash
  BlockValidationResult CheckCoinstakeTransaction(CTransactionRef tx) const {
    BlockValidationResult validationErrors;

    if (tx->vin.size() < 1) {
      validationErrors += Error::NO_META_INPUT;
    } else {
      validationErrors += CheckCoinstakeMetaInput(tx->vin[0]);
    }
    if (tx->vin.size() < 2) {
      validationErrors += Error::NO_STAKING_INPUT;
    }
    if (tx->vout.empty()) {
      validationErrors += Error::COINSTAKE_TRANSACTION_WITHOUT_OUTPUT;
    }
    return validationErrors;
  }

  //! \brief checks that the first input of a coinstake transaction is well-formed
  //!
  //! A well-formed meta input encodes the block height, followed by the snapshot hash.
  //! It is then either terminated by OP_0 or some data follows (forwards-compatible).
  BlockValidationResult CheckCoinstakeMetaInput(const CTxIn &in) const {
    BlockValidationResult validationErrors;

    const CScript &scriptSig = in.scriptSig;

    bool check;
    opcodetype op;
    std::vector<uint8_t> buf;
    CScript::const_iterator it = scriptSig.begin();

    // read + check height
    check = scriptSig.GetOp(it, op, buf);
    if (!check || buf.empty()) {
      validationErrors += Error::NO_BLOCK_HEIGHT;
      validationErrors += Error::NO_SNAPSHOT_HASH;
      return validationErrors;
    }
    try {
      CScriptNum height(buf, true);
    } catch (scriptnum_error &) {
      validationErrors += Error::INVALID_BLOCK_HEIGHT;
    }

    // read + check snapshot hash
    check = scriptSig.GetOp(it, op, buf);
    if (!check || op != 0x20 || buf.size() != 32) {
      validationErrors += Error::NO_SNAPSHOT_HASH;
      return validationErrors;
    }

    return validationErrors;
  }

  //! \brief checks the proposer signature of the block
  BlockValidationResult CheckBlockSignature(const CBlock &block) const {
    BlockValidationResult validationErrors;

    if (block.vtx.empty()) {
      validationErrors += Error::NO_TRANSACTIONS;
      return validationErrors;
    }
    if (block.vtx[0]->vin.size() < 2) {
      validationErrors += Error::NO_STAKING_INPUT;
      return validationErrors;
    }
    if (block.vtx[0]->vin[1].scriptWitness.stack.size() != 2) {
      validationErrors += Error::NO_BLOCK_PUBLIC_KEY;
      return validationErrors;
    }

    const auto &signature = block.signature;
    const auto &witnessStack = block.vtx[0]->vin[1].scriptWitness.stack;

    const auto &pubKeyData = witnessStack[1];
    CPubKey key(pubKeyData.begin(), pubKeyData.end());

    if (!key.IsValid()) {
      validationErrors += Error::INVALID_BLOCK_PUBLIC_KEY;
      return validationErrors;
    }

    const uint256 blockHash = block.GetHash();

    if (!key.Verify(blockHash, signature)) {
      validationErrors += Error::BLOCK_SIGNATURE_VERIFICATION_FAILED;
    }
    return validationErrors;
  }

 public:
  BlockValidationResult CheckBlock(const CBlock &block) const override {
    BlockValidationResult validationErrors;

    // check that there are transactions
    if (block.vtx.empty()) {
      validationErrors += Error::NO_TRANSACTIONS;
      return validationErrors;
    }

    // check that coinstake transaction is first transaction
    if (block.vtx[0]->GetType() == +TxType::COINSTAKE) {
      validationErrors += CheckCoinstakeTransaction(block.vtx[0]);
    } else {
      validationErrors += Error::FIRST_TRANSACTION_NOT_A_COINSTAKE_TRANSACTION;
    }

    // check that all other transactions are no coinstake transactions
    for (auto tx = block.vtx.cbegin() + 1; tx != block.vtx.cend(); ++tx) {
      if ((*tx)->GetType() == +TxType::COINSTAKE) {
        validationErrors += Error::COINSTAKE_TRANSACTION_AT_POSITION_OTHER_THAN_FIRST;
      }
    }

    bool duplicateTransactions;

    // check merkle root
    const uint256 expectedMerkleRoot = BlockMerkleRoot(block, &duplicateTransactions);
    if (block.hashMerkleRoot != expectedMerkleRoot) {
      validationErrors += Error::MERKLE_ROOT_MISMATCH;
    }
    if (duplicateTransactions) {
      // UNIT-E TODO: this check is required to mitigate CVE-2012-2459
      // Apparently an alternative construction of the merkle tree avoids this
      // issue completely _and_ results in faster merkle tree construction, see
      // BIP 98 https://github.com/bitcoin/bips/blob/master/bip-0098.mediawiki
      validationErrors += Error::DUPLICATE_TRANSACTIONS_IN_MERKLE_TREE;
    }

    // check witness merkle root
    const uint256 expectedWitnessMerkleRoot = BlockWitnessMerkleRoot(block, &duplicateTransactions);
    // UNIT-E TODO: in #212 the witness merkle root will be part of the block header
    //if (block.hashWitnessMerkleRoot != expectedWitnessMerkleRoot) {
    //  validationErrors += Error::WITNESS_MERKLE_ROOT_MISMATCH;
    //}
    if (duplicateTransactions) {
      validationErrors += Error::DUPLICATE_TRANSACTIONS_IN_WITNESS_MERKLE_TREE;
    }

    // check proposer signature
    validationErrors += CheckBlockSignature(block);

    return validationErrors;
  }
};

std::unique_ptr<BlockValidator> BlockValidator::New() {
  return std::unique_ptr<BlockValidator>(new BlockValidatorImpl());
};

}  // namespace staking
