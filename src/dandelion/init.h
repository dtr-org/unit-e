// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_DANDELION_INIT_H
#define UNITE_DANDELION_INIT_H

#include <dandelion/dandelion.h>
#include <net.h>

namespace dandelion {

// UNIT-E: TODO: adjust default parameters once we have a testnet
class Params {
 public:
  bool enabled = true;

  //! \brief minimum embargo time
  std::chrono::seconds embargoMin = std::chrono::seconds(5);

  //! \breif average embargo time that is added to embargoMin
  std::chrono::seconds embargoAvgAdd = std::chrono::seconds(2);

  //! \brief minimum numbers of fluffs to switch relay
  //! If our relay turns out to be a black hole - attempt to switch it after so
  //! many embargo timeouts
  size_t timeoutsToSwitchRelay = 2;

  static const Params Create(ArgsManager &args);
  static std::string GetHelpString();
};

std::unique_ptr<DandelionLite> CreateDandelion(CConnman &connman,
                                               const Params &params);

}  // namespace dandelion

#endif  //UNITE_DANDELION_INIT_H
