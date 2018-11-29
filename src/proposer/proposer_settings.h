// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_PROPOSER_SETTINGS_H
#define UNIT_E_PROPOSER_PROPOSER_SETTINGS_H

#include <amount.h>
#include <dependency.h>
#include <util.h>

#include <cstddef>
#include <memory>

namespace proposer {

struct Settings {

  //! \brief whether to actively propose or not
  bool proposing = true;

  //! \brief number of threads to use for proposing
  size_t number_of_proposer_threads = 1;

  std::chrono::milliseconds proposer_sleep = std::chrono::seconds(30);

  //! \brief minimum interval between proposing blocks
  std::chrono::milliseconds min_propose_interval = std::chrono::seconds(4);

  std::string proposer_thread_prefix = "proposer";

  CAmount stake_combine_threshold = 1000 * UNIT;

  CAmount stake_split_threshold = 1000 * UNIT;

  //! \brief maximum number of coins to combine when staking
  size_t max_stake_combine = 10;

  static std::unique_ptr<Settings> New(Dependency<Ptr<ArgsManager>> args);
};

}  // namespace proposer

#endif  // UNIT_E_PROPOSER_SETTINGS_H
