// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/network.h>

#include <net.h>
#include <timedata.h>

namespace proposer {

class NetworkAdapter : public Network {

 public:
  int64_t GetTime() const override { return GetAdjustedTime(); }

  size_t GetNodeCount() override { return g_connman->GetNodeCount(); }

  size_t GetInboundNodeCount() override {
    return g_connman->GetNodeCount(CConnman::CONNECTIONS_IN);
  }

  size_t GetOutboundNodeCount() override {
    return g_connman->GetNodeCount(CConnman::CONNECTIONS_OUT);
  }
};

std::unique_ptr<Network> Network::New() {
  return std::unique_ptr<Network>(new NetworkAdapter());
}

}  // namespace proposer
