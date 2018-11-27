// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer_settings.h>

#include <util.h>

#include <memory>

namespace proposer {

std::unique_ptr<Settings> Settings::New(Dependency<Ptr<ArgsManager>> args) {
  std::unique_ptr<Settings> settings = MakeUnique<Settings>();

  settings->proposing = args->obj->GetBoolArg("-proposing", settings->proposing);

  // UNIT-E TODO: allow setting other settings from command line

  return settings;
}

}  // namespace proposer
