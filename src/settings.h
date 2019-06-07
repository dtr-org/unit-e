// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SETTINGS_H
#define UNITE_SETTINGS_H

#include <amount.h>
#include <dependency.h>
#include <util.h>

#include <script/standard.h>
#include <memory>

namespace blockchain {
class Behavior;
}

struct Settings {

  //! Whether this node should act as a validator or not.
  bool node_is_validator = false;

  //! Whether to participate in proposing new blocks or not.
  bool node_is_proposer = true;

  //! \brief Maximum amount that a single coinbase output should have.
  //!
  //! When proposing the proposer will combine eligible coins with respect
  //! to stake_combine_maximum and form a coinbase transaction with a single
  //! output. If stake_split_threshold is greater than zero it will split the
  //! coinbase outputs into pieces that are no larger than that.
  CAmount stake_split_threshold = 0;

  //! \brief Maximum amount of money to combine when proposing.
  //!
  //! When proposing the proposer will combine the eligible coins with respect
  //! to the total amount. The only guarantee is that the combined stake
  //! will not exceed this value in total, not that it's optimal (that would
  //! require solving the Knapsack problem otherwise).
  CAmount stake_combine_maximum = 0;

  std::uint16_t p2p_port = 7182;

  //! \brief Path to the base data dir (e.g. ~user/.unit-e).
  fs::path base_data_dir = GetDefaultDataDir();

  //! \brief Path to the data dir (e.g. ~user/.unit-e/regtest).
  fs::path data_dir = GetDefaultDataDir();

  //! \brief From which block in the epoch finalizer must start voting
  //!
  //! It can make sense to start voting a bit later than after the checkpoint
  //! is processed as if re-orgs happens on a checkpoint and finalizer switches to
  //! that fork, it can't vote on this epoch again as it will be double-voting.
  //! Delaying the vote reduces the chance of voting on the fork
  //! that won't be final for the finalizer but delaying for too long
  //! can risk that vote won't be included by proposer at all.
  //!
  //! see default values in Parameters.finalizer_vote_on_epoch_block_number
  std::uint32_t finalizer_vote_from_epoch_block_number = 0;

  //! \brief the destination of the proposing reward.
  //
  //! If not set it will use the destination of the coin used for proposing the
  //! block.
  boost::optional<CTxDestination> reward_destination = boost::none;

  static std::unique_ptr<Settings> New(
      Dependency<::ArgsManager>,
      Dependency<blockchain::Behavior>);
};

#endif  // UNITE_SETTINGS_H
