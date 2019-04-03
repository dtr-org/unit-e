// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_SETTINGS_H
#define UNIT_E_SETTINGS_H

#include <amount.h>
#include <dependency.h>
#include <fs.h>
#include <script/standard.h>
#include <util.h>

#include <boost/optional.hpp>

#include <memory>

namespace blockchain {
class Behavior;
}

namespace staking {

struct ReturnStakeToSameAddress {};
struct ReturnStakeToNewAddress {};

using StakeReturnMode = boost::variant<ReturnStakeToSameAddress, ReturnStakeToNewAddress, CScript>;

}  // namespace staking

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

  std::uint16_t p2p_port = 7182;

  //! \brief Path to the base data dir (e.g. ~/user/.unite).
  fs::path base_data_dir = GetDefaultDataDir();

  //! \brief Path to the data dir (e.g. ~/user/.unite/regtest).
  fs::path data_dir = GetDefaultDataDir();

  //! \brief Destination to send the reward for proposing a block to.
  //!
  //! If not set it will use the destination of the coin used for proposing the
  //! block.
  boost::optional<CTxDestination> reward_destination = boost::none;

  staking::StakeReturnMode stake_return_mode = staking::ReturnStakeToSameAddress{};

  static std::unique_ptr<Settings> New(
      Dependency<::ArgsManager>,
      Dependency<blockchain::Behavior>);
};

#endif  // UNIT_E_SETTINGS_H
