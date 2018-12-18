// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <settings.h>

#include <dependency.h>

std::unique_ptr<Settings> Settings::New(Dependency<::ArgsManager> args) {
  std::unique_ptr<Settings> settings = MakeUnique<Settings>();

  settings->node_is_proposer =
      args->GetBoolArg("-proposing", settings->node_is_proposer);

  settings->node_is_validator =
      args->GetBoolArg("-validating", settings->node_is_validator);

  settings->stake_combine_maximum =
      args->GetArg("-stakecombinemaximum", settings->stake_combine_maximum);

  settings->stake_split_threshold =
      args->GetArg("-stakesplitthreshold", settings->stake_split_threshold);

  return settings;
}
