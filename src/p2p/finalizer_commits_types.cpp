// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/finalizer_commits_types.h>
#include <util.h>

std::string p2p::FinalizerCommitsLocator::ToString() const {
  return strprintf("Locator(start=%s, stop=%s)", util::to_string(start), stop.GetHex());
}
