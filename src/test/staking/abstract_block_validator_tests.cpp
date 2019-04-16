// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/block_validator.h>

#include <blockchain/blockchain_genesis.h>
#include <blockchain/blockchain_parameters.h>
#include <consensus/merkle.h>
#include <key/mnemonic/mnemonic.h>
#include <test/test_unite.h>
#include <timedata.h>

#include <boost/test/unit_test.hpp>

namespace {

using Func = std::function<void(staking::BlockValidationResult &)>;

class SomeBlockValidator : public staking::AbstractBlockValidator {

 public:
  mutable std::size_t count_CheckBlockHeaderInternal = 0;
  mutable std::size_t count_ContextualCheckBlockHeaderInternal = 0;
  mutable std::size_t count_CheckBlockInternal = 0;
  mutable std::size_t count_ContextualCheckBlockInternal = 0;

  std::unique_ptr<Func> func_CheckBlockHeaderInternal = nullptr;
  std::unique_ptr<Func> func_ContextualCheckBlockHeaderInternal = nullptr;
  std::unique_ptr<Func> func_CheckBlockInternal = nullptr;
  std::unique_ptr<Func> func_ContextualCheckBlockInternal = nullptr;

  void CheckBlockHeaderInternal(
      const CBlockHeader &block_header,
      staking::BlockValidationResult &result) const override {
    ++count_CheckBlockHeaderInternal;
    if (func_CheckBlockHeaderInternal) {
      (*func_CheckBlockHeaderInternal)(result);
    }
  }

  void ContextualCheckBlockHeaderInternal(
      const CBlockHeader &block_header,
      blockchain::Time adjusted_time,
      const CBlockIndex &previous_block,
      staking::BlockValidationResult &result) const override {
    ++count_ContextualCheckBlockHeaderInternal;
    if (func_ContextualCheckBlockHeaderInternal) {
      (*func_ContextualCheckBlockHeaderInternal)(result);
    }
  }

  void CheckBlockInternal(
      const CBlock &block,
      blockchain::Height *height_out,
      uint256 *snapshot_hash_out,
      staking::BlockValidationResult &result) const override {
    ++count_CheckBlockInternal;
    if (func_CheckBlockInternal) {
      (*func_CheckBlockInternal)(result);
    }
  }

  void ContextualCheckBlockInternal(
      const CBlock &block,
      const CBlockIndex &prev_block,
      const staking::BlockValidationInfo &validation_info,
      staking::BlockValidationResult &result) const override {
    ++count_ContextualCheckBlockInternal;
    if (func_ContextualCheckBlockInternal) {
      (*func_ContextualCheckBlockInternal)(result);
    }
  }

  staking::BlockValidationResult CheckCoinbaseTransaction(const CTransaction &coinbase_tx) const override {
    return staking::BlockValidationResult();
  }
};

using Error = staking::BlockValidationError;

}  // namespace

BOOST_AUTO_TEST_SUITE(abstract_block_validator_tests)

BOOST_AUTO_TEST_CASE(check_block_header_test) {

  const CBlock block;
  const CBlockIndex prev_block;

  {
    SomeBlockValidator v;
    bool result = static_cast<bool>(v.CheckBlockHeader(block, nullptr));
    BOOST_CHECK_EQUAL(v.count_CheckBlockHeaderInternal, 1);
    BOOST_CHECK(result);
  }

  {
    SomeBlockValidator v;
    v.func_CheckBlockHeaderInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.CheckBlockHeader(block, nullptr));
    BOOST_CHECK_EQUAL(v.count_CheckBlockHeaderInternal, 1);
    BOOST_CHECK(!result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsUnknown());
    bool result = static_cast<bool>(v.CheckBlockHeader(block, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockHeaderInternal, 1);
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsTrue());
    BOOST_CHECK(result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsUnknown());
    v.func_CheckBlockHeaderInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.CheckBlockHeader(block, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockHeaderInternal, 1);
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsFalse());
    BOOST_CHECK(!result);
  }
}

BOOST_AUTO_TEST_CASE(contextual_check_block_header_test) {

  CBlockIndex prev_block;
  prev_block.nTime = 0;
  CBlock block;
  block.nTime = prev_block.nTime + 16;

  {
    SomeBlockValidator v;
    bool result = static_cast<bool>(v.ContextualCheckBlockHeader(block, prev_block, 0, nullptr));
    BOOST_CHECK_EQUAL(v.count_CheckBlockHeaderInternal, 1);
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockHeaderInternal, 1);
    BOOST_CHECK(result);
  }

  {
    SomeBlockValidator v;
    v.func_CheckBlockHeaderInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.ContextualCheckBlockHeader(block, prev_block, 0, nullptr));
    BOOST_CHECK_EQUAL(v.count_CheckBlockHeaderInternal, 1);
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockHeaderInternal, 0);
    BOOST_CHECK(!result);
  }

  {
    SomeBlockValidator v;
    v.func_ContextualCheckBlockHeaderInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.ContextualCheckBlockHeader(block, prev_block, 0, nullptr));
    BOOST_CHECK_EQUAL(v.count_CheckBlockHeaderInternal, 1);
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockHeaderInternal, 1);
    BOOST_CHECK(!result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsUnknown());
    BOOST_CHECK(i.GetContextualCheckBlockHeaderStatus().IsUnknown());
    bool result = static_cast<bool>(v.ContextualCheckBlockHeader(block, prev_block, block.nTime, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockHeaderInternal, 1);
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsTrue());
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockHeaderInternal, 1);
    BOOST_CHECK(i.GetContextualCheckBlockHeaderStatus().IsTrue());
    BOOST_CHECK(result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsUnknown());
    BOOST_CHECK(i.GetContextualCheckBlockHeaderStatus().IsUnknown());
    v.func_ContextualCheckBlockHeaderInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.ContextualCheckBlockHeader(block, prev_block, block.nTime, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockHeaderInternal, 1);
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsTrue());
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockHeaderInternal, 1);
    BOOST_CHECK(i.GetContextualCheckBlockHeaderStatus().IsFalse());
    BOOST_CHECK(!result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    i.MarkCheckBlockHeaderSuccessfull();
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsTrue());
    BOOST_CHECK(i.GetContextualCheckBlockHeaderStatus().IsUnknown());
    bool result = static_cast<bool>(v.ContextualCheckBlockHeader(block, prev_block, 0, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockHeaderInternal, 0);
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsTrue());
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockHeaderInternal, 1);
    BOOST_CHECK(i.GetContextualCheckBlockHeaderStatus().IsTrue());
    BOOST_CHECK(result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsUnknown());
    BOOST_CHECK(i.GetContextualCheckBlockHeaderStatus().IsUnknown());
    v.func_ContextualCheckBlockHeaderInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.ContextualCheckBlockHeader(block, prev_block, block.nTime, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockHeaderInternal, 1);
    BOOST_CHECK(i.GetCheckBlockHeaderStatus().IsTrue());
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockHeaderInternal, 1);
    BOOST_CHECK(i.GetContextualCheckBlockHeaderStatus().IsFalse());
    BOOST_CHECK(!result);
  }
}

BOOST_AUTO_TEST_CASE(check_block_test) {

  const CBlock block;
  const CBlockIndex prev_block;

  {
    SomeBlockValidator v;
    bool result = static_cast<bool>(v.CheckBlock(block, nullptr));
    BOOST_CHECK_EQUAL(v.count_CheckBlockInternal, 1);
    BOOST_CHECK(result);
  }

  {
    SomeBlockValidator v;
    v.func_CheckBlockInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.CheckBlock(block, nullptr));
    BOOST_CHECK_EQUAL(v.count_CheckBlockInternal, 1);
    BOOST_CHECK(!result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    BOOST_CHECK(i.GetCheckBlockStatus().IsUnknown());
    bool result = static_cast<bool>(v.CheckBlock(block, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockInternal, 1);
    BOOST_CHECK(i.GetCheckBlockStatus().IsTrue());
    BOOST_CHECK(result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    BOOST_CHECK(i.GetCheckBlockStatus().IsUnknown());
    v.func_CheckBlockInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.CheckBlock(block, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockInternal, 1);
    BOOST_CHECK(i.GetCheckBlockStatus().IsFalse());
    BOOST_CHECK(!result);
  }
}

BOOST_AUTO_TEST_CASE(contextual_check_block_test) {

  CBlockIndex prev_block;
  prev_block.nTime = 0;
  CBlock block;
  block.nTime = prev_block.nTime + 16;

  {
    SomeBlockValidator v;
    bool result = static_cast<bool>(v.ContextualCheckBlock(block, prev_block, 0, nullptr));
    BOOST_CHECK_EQUAL(v.count_CheckBlockInternal, 1);
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockInternal, 1);
    BOOST_CHECK(result);
  }

  {
    SomeBlockValidator v;
    v.func_CheckBlockInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.ContextualCheckBlock(block, prev_block, 0, nullptr));
    BOOST_CHECK_EQUAL(v.count_CheckBlockInternal, 1);
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockInternal, 0);
    BOOST_CHECK(!result);
  }

  {
    SomeBlockValidator v;
    v.func_ContextualCheckBlockInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.ContextualCheckBlock(block, prev_block, 0, nullptr));
    BOOST_CHECK_EQUAL(v.count_CheckBlockInternal, 1);
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockInternal, 1);
    BOOST_CHECK(!result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    BOOST_CHECK(i.GetCheckBlockStatus().IsUnknown());
    BOOST_CHECK(i.GetContextualCheckBlockStatus().IsUnknown());
    bool result = static_cast<bool>(v.ContextualCheckBlock(block, prev_block, block.nTime, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockInternal, 1);
    BOOST_CHECK(i.GetCheckBlockStatus().IsTrue());
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockInternal, 1);
    BOOST_CHECK(i.GetContextualCheckBlockStatus().IsTrue());
    BOOST_CHECK(result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    BOOST_CHECK(i.GetCheckBlockStatus().IsUnknown());
    BOOST_CHECK(i.GetContextualCheckBlockStatus().IsUnknown());
    v.func_ContextualCheckBlockInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.ContextualCheckBlock(block, prev_block, block.nTime, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockInternal, 1);
    BOOST_CHECK(i.GetCheckBlockStatus().IsTrue());
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockInternal, 1);
    BOOST_CHECK(i.GetContextualCheckBlockStatus().IsFalse());
    BOOST_CHECK(!result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    i.MarkCheckBlockSuccessfull(1, uint256());
    BOOST_CHECK(i.GetCheckBlockStatus().IsTrue());
    BOOST_CHECK(i.GetContextualCheckBlockStatus().IsUnknown());
    bool result = static_cast<bool>(v.ContextualCheckBlock(block, prev_block, 0, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockInternal, 0);
    BOOST_CHECK(i.GetCheckBlockStatus().IsTrue());
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockInternal, 1);
    BOOST_CHECK(i.GetContextualCheckBlockStatus().IsTrue());
    BOOST_CHECK(result);
  }

  {
    SomeBlockValidator v;
    staking::BlockValidationInfo i;
    BOOST_CHECK(i.GetCheckBlockStatus().IsUnknown());
    BOOST_CHECK(i.GetContextualCheckBlockStatus().IsUnknown());
    v.func_ContextualCheckBlockInternal = MakeUnique<Func>([](staking::BlockValidationResult &r) {
      r.AddError(staking::BlockValidationError::INVALID_BLOCK_TIME);
    });
    bool result = static_cast<bool>(v.ContextualCheckBlock(block, prev_block, block.nTime, &i));
    BOOST_CHECK_EQUAL(v.count_CheckBlockInternal, 1);
    BOOST_CHECK(i.GetCheckBlockStatus().IsTrue());
    BOOST_CHECK_EQUAL(v.count_ContextualCheckBlockInternal, 1);
    BOOST_CHECK(i.GetContextualCheckBlockStatus().IsFalse());
    BOOST_CHECK(!result);
  }
}

BOOST_AUTO_TEST_SUITE_END()
