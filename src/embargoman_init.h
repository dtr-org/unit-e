// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_EMBARGOMAN_INIT_H
#define UNITE_EMBARGOMAN_INIT_H

#include <chrono>
#include <memory>
#include <set>

#include <embargoman.h>
#include <net.h>

namespace p2p {

// UNIT-E: TODO: adjust default parameters once we have a testnet
class EmbargoManParams {
 public:
  bool enabled = true;

  //! \brief minimum embargo time
  std::chrono::seconds embargoMin = std::chrono::seconds(5);

  //! \brief average embargo time that is added to embargoMin
  std::chrono::seconds embargoAvgAdd = std::chrono::seconds(2);

  //! \brief minimum numbers of fluffs to switch relay
  //! If our relay turns out to be a black hole - attempt to switch it after so
  //! many embargo timeouts
  size_t timeoutsToSwitchRelay = 2;

  static bool Create(ArgsManager &args,
                     p2p::EmbargoManParams &paramsOut,
                     std::string &errorMessageOut);
  static std::string GetHelpString();
};

std::unique_ptr<EmbargoMan> CreateEmbargoMan(CConnman &connman,
                                             const EmbargoManParams &params);

}  // namespace network

#endif  //UNITE_EMBARGOMAN_INIT_H
