// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/network.h>

#include <net.h>
#include <timedata.h>

namespace staking {

class NetworkAdapter : public Network {

 public:
  int64_t GetTime() const override { return GetAdjustedTime(); }

  size_t GetNodeCount() const override { return g_connman->GetNodeCount(); }

  size_t GetInboundNodeCount() const override {
    return g_connman->GetNodeCount(CConnman::CONNECTIONS_IN);
  }

  size_t GetOutboundNodeCount() const override {
    return g_connman->GetNodeCount(CConnman::CONNECTIONS_OUT);
  }
};

std::unique_ptr<Network> Network::New() {
  return std::unique_ptr<Network>(new NetworkAdapter());
}

}  // namespace staking
