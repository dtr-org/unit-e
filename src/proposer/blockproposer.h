// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_BLOCKPROPOSER_H
#define UNIT_E_PROPOSER_BLOCKPROPOSER_H

#include <proposer/chainstate.h>

#include <dependency.h>
#include <primitives/transaction.h>
#include <proposer/transactionpicker.h>
#include <pubkey.h>
#include <staking/stakingwallet.h>
#include <boost/optional.hpp>

#include <stdint.h>
#include <memory>

class CBlock;
class CWallet;
class CWalletTx;

namespace proposer {

//! \brief a component for proposing new blocks.
//!
//! The block proposer will build and propose a block, given a wallet
//! that has enough stake.
//!
//! The BlockProposer is different from the Proposer in proposer.cpp.
//! That one is managing concurrency (number of staking threads),
//! availability of wallets, balance, etc. The BlockProposer is used
//! to actually propose a block once we know that we have the means to
//! do so.
//!
//! This class is an interface.
class BlockProposer {

 public:
  struct ProposeBlockParameters {
    //! \brief the height to propose the block for.
    //!
    //! The height of a block is encoded inside the coinbase transaction.
    //! This is defined in BIP34 (Block v2, Height in Coinbase).
    //! This also helps ensuring that the coinbase transaction has a
    //! unique hash, hence prevent BIP30 (Duplicate transactions) from
    //! applying.
    //!
    //! Block height is up to 2^31 which is enough to support one block
    //! every second for 68 years. This is because block height used to
    //! be signed and the signbit is now overloaded in some places to
    //! signal a coinstake transaction in various serializations of
    //! coins / UTXOs.
    uint32_t blockHeight;

    //! \brief the block time to propose the block with.
    //!
    //! The time is a regular unix timestamp in seconds.
    int64_t blockTime;

    //! \brief the wallet to draw funds for staking from.
    //!
    //! The stake to propose with is drawn from the given wallet. The
    //! amount of stake will increase the chances of proposing since
    //! a certain difficulty threshold has to be met in order to do so.
    staking::StakingWallet *wallet;
  };

  virtual std::shared_ptr<const CBlock> ProposeBlock(
      const ProposeBlockParameters &) = 0;

  virtual ~BlockProposer() = default;

  //! \brief Factory method for creating a BlockProposer
  static std::unique_ptr<BlockProposer> MakeBlockProposer(
      Dependency<ChainState>, Dependency<TransactionPicker>);
};

}  // namespace proposer

#endif  // UNIT_E_PROPOSER_BLOCKPROPOSER_H
