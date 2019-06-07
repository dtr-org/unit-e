// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#ifndef UNITE_STAKING_BLOCK_VALIDATION_INFO_H
#define UNITE_STAKING_BLOCK_VALIDATION_INFO_H

#include <trit.h>

#include <blockchain/blockchain_types.h>

namespace staking {

//! \brief Validation related information about a block.
//!
//! There's a sea of validation functions available to check blocks at
//! different states of completeness. Nevertheless we never want to miss
//! a check for correctness reasons, but we also do not want to perform
//! any checks twice (for efficiency/performance reasons). In order to
//! orchestrate that gracefully instances of this class can track the
//! state of certain validations.
//!
//! This was inspired by CBlock::fCheck introduced in bitcoin around 2012.
class BlockValidationInfo {

 private:
  //! \brief Whether CheckBlockHeader validated the block's header (and if so whether successful).
  Trit m_check_CheckBlockHeader;

  //! \brief Whether ContextualCheckBlockHeader validated the block's header (and if so whether successful).
  Trit m_check_ContextualCheckBlockHeader;

  //! \brief Whether CheckBlock validated this block (and if so whether successful).
  Trit m_check_CheckBlock;

  //! \brief Whether ContextualCheckBlock validated this block (and if so whether successful).
  Trit m_check_ContextualCheckBlock;

  //! \brief Whether CheckStake validated this block (and if so whether successful).
  Trit m_check_CheckStake;

  //! \brief The height as parsed from the coinbases scriptSig, if m_check_CheckBlock succeeded.
  blockchain::Height m_height = 0;

  //! \brief The snapshot hash as parsed form the coinbases scriptSig, if m_check_CheckBlock succeeded.
  uint256 m_snapshot_hash;

 public:
  explicit operator bool() const noexcept {
    return static_cast<bool>(Trit::And(
        m_check_CheckBlockHeader,
        m_check_ContextualCheckBlockHeader,
        m_check_CheckBlock,
        m_check_ContextualCheckBlock,
        m_check_CheckStake));
  }

  //! \brief Marks that CheckBlockHeader() validated the block successfully.
  //!
  //! Further invocations of ContextualCheckBlock may return true immediately.
  void MarkCheckBlockHeaderSuccessfull() noexcept {
    m_check_CheckBlockHeader = Trit::True;
  }

  //! \brief Marks that CheckBlockHeader() failed to validate the block.
  //!
  //! Further invocations of ContextualCheckBlock may return false immediately.
  void MarkCheckBlockHeaderFailed() noexcept {
    m_check_CheckBlockHeader = Trit::False;
  }

  //! \brief Marks that ContextualCheckBlockHeader() validated the block successfully.
  //!
  //! Further invocations of ContextualCheckBlock may return true immediately.
  void MarkContextualCheckBlockHeaderSuccessfull() noexcept {
    m_check_ContextualCheckBlockHeader = Trit::True;
  }

  //! \brief Marks that ContextualCheckBlockHeader() failed to validate the block.
  //!
  //! Further invocations of ContextualCheckBlock may return false immediately.
  void MarkContextualCheckBlockHeaderFailed() noexcept {
    m_check_ContextualCheckBlockHeader = Trit::False;
  }

  //! \brief Marks that CheckBlock() validated the block successfully.
  //!
  //! When CheckBlock() successfully validated a block, it will also come
  //! across height and snapshot_hash from the coinbase transaction. These
  //! can be re-used in contextual check block to check that they match what
  //! we know about the previous block.
  //!
  //! Further invocations of CheckBlock() may return immediately.
  void MarkCheckBlockSuccessfull(
      const blockchain::Height &height,
      const uint256 &snapshot_hash) noexcept {
    m_height = height;
    m_snapshot_hash = snapshot_hash;
    m_check_CheckBlock = Trit::True;
  }

  //! \brief Marks that CheckBlock() failed to validate the block.
  //!
  //! Further invocations of CheckBlock may return false immediately.
  void MarkCheckBlockFailed() noexcept {
    m_check_CheckBlock = Trit::False;
  }

  //! \brief Marks that ContextualCheckBlock() validated the block successfully.
  //!
  //! Further invocations of ContextualCheckBlock may return true immediately.
  void MarkContextualCheckBlockSuccessfull() noexcept {
    m_check_ContextualCheckBlock = Trit::True;
  }

  //! \brief Marks that ContextualCheckBlock() failed to validate the block.
  //!
  //! Further invocations of ContextualCheckBlock may return false immediately.
  void MarkContextualCheckBlockFailed() noexcept {
    m_check_ContextualCheckBlock = Trit::False;
  }

  //! \brief Marks that CheckStake() validated the block successfully.
  //!
  //! Further invocations of ContextualCheckBlock may return true immediately.
  void MarkCheckStakeSuccessfull() noexcept {
    m_check_CheckStake = Trit::True;
  }

  //! \brief Marks that CheckStake() failed to validate the block.
  //!
  //! Further invocations of CheckStake() may return false immediately.
  void MarkCheckStakeFailed() noexcept {
    m_check_CheckStake = Trit::False;
  }

  //! \brief Retrieves the status of the CheckBlock() check.
  Trit GetCheckBlockHeaderStatus() const noexcept {
    return m_check_CheckBlockHeader;
  }

  //! \brief Retrieves the status of the ContextualCheckBlockHeader() check.
  Trit GetContextualCheckBlockHeaderStatus() const noexcept {
    return m_check_ContextualCheckBlockHeader;
  }

  //! \brief Retrieves the status of the CheckBlock() check.
  Trit GetCheckBlockStatus() const noexcept {
    return m_check_CheckBlock;
  }

  //! \brief Retrieves the status of the ContextualCheckBlock() check.
  Trit GetContextualCheckBlockStatus() const noexcept {
    return m_check_ContextualCheckBlock;
  }

  //! \brief Retrieves the status of the ContextualCheckBlock() check.
  Trit GetCheckStakeStatus() const noexcept {
    return m_check_CheckStake;
  }

  //! \brief Retrieves the height of the block as parsed during CheckBlock().
  //!
  //! This value is available if and only if `GetCheckBlockStatus().IsTrue()`,
  //! otherwise a junk value (undefined) is returned.
  blockchain::Height GetHeight() const noexcept {
    return m_height;
  }

  //! \brief Retrieves the snapshot hash of the block as parsed during CheckBlock().
  //!
  //! This value is available if and only if `GetCheckBlockStatus().IsTrue()`,
  //! otherwise a junk value (undefined) is returned.
  uint256 GetSnapshotHash() const noexcept {
    return m_snapshot_hash;
  }
};

}  // namespace staking

#endif  // UNITE_STAKING_BLOCK_VALIDATION_INFO_H
