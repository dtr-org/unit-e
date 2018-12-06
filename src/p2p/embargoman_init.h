// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_EMBARGOMAN_INIT_H
#define UNITE_P2P_EMBARGOMAN_INIT_H

#include <chrono>
#include <memory>
#include <set>

#include <net.h>
#include <p2p/embargoman.h>

namespace p2p {

// UNIT-E: TODO: adjust default parameters once we have a testnet
class EmbargoManParams {
 public:
  bool enabled = true;

  //! \brief minimum embargo time
  std::chrono::seconds embargo_min = std::chrono::seconds(5);

  //! \brief average embargo time that is added to embargoMin
  std::chrono::seconds embargo_avg_add = std::chrono::seconds(2);

  //! \brief minimum numbers of fluffs to switch relay
  //! If our relay turns out to be a black hole - attempt to switch it after so
  //! many embargo timeouts
  size_t timeouts_to_switch_relay = 2;

  static bool Create(const ArgsManager &args,
                     p2p::EmbargoManParams &params_out,
                     std::string &error_message_out);

  EmbargoManParams() = default;
};

std::unique_ptr<EmbargoMan> CreateEmbargoMan(CConnman &connman,
                                             const EmbargoManParams &params);

}  // namespace p2p

#endif  //UNITE_P2P_EMBARGOMAN_INIT_H
