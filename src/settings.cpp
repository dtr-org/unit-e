// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <settings.h>

#include <dependency.h>
#include <base58.h>

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

  const std::string reward_address = args->GetArg("-rewardaddress", "");
  if (!reward_address.empty()) {
    CTxDestination reward_dest = DecodeDestination(reward_address);
    if (IsValidDestination(reward_dest)) {
      settings->reward_destination = std::move(reward_dest);
    } else {
      settings->reward_destination = boost::none;
      LogPrintf("-rewardaddress: Invalid address provided %s\n", __func__, reward_address);
    }
  }

  return settings;
}
