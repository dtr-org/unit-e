// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_SETTINGS_H
#define UNIT_E_SETTINGS_H

#include <amount.h>
#include <dependency.h>
#include <util.h>

#include <memory>

struct Settings {

  //! Whether this node should act as a validator or not.
  bool node_is_validator = false;

  //! Whether to participate in proposing new blocks or not.
  bool node_is_proposer = true;

  //! Number of pieces stake should be split into when proposing.
  CAmount stake_split_threshold = 1;

  //! Maximum numbers of coin to combine when proposing.
  CAmount stake_combine_maximum = 0;

  static std::unique_ptr<Settings> New(Dependency<::ArgsManager>);
};

#endif // UNIT_E_SETTINGS_H
