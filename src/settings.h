// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_SETTINGS_H
#define UNIT_E_SETTINGS_H

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
  //! to the total amount. The only guarantee is that it the combined stake
  //! will not exceed this value in total, not that it's optimal (that would
  //! require solving the Knapsack problem otherwise).
  CAmount stake_combine_maximum = 0;

  //! \brief the destination of the proposing reward.
  //
  //! If not set it will use the destination of the coin used for proposing the
  //! block.
  boost::optional<CTxDestination> reward_destination = boost::none;

  static std::unique_ptr<Settings> New(
      Dependency<::ArgsManager>,
      Dependency<blockchain::Behavior>);
};

#endif // UNIT_E_SETTINGS_H
