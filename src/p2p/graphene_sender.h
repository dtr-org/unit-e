// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_GRAPHENE_SENDER_H
#define UNITE_P2P_GRAPHENE_SENDER_H

#include <unordered_map>

#include <dependency.h>
#include <net.h>
#include <p2p/graphene.h>
#include <p2p/graphene_messages.h>
#include <primitives/block.h>

namespace p2p {

class GrapheneSender {
 public:
  virtual void UpdateRequesterTxPoolCount(const CNode &requester, uint64_t new_count) = 0;
  virtual bool SendBlock(CNode &to, const CBlock &block, const CBlockIndex &index) = 0;
  virtual void OnGrapheneTxRequestReceived(CNode &from,
                                           const GrapheneTxRequest &request) = 0;
  virtual void OnDisconnected(NodeId node) = 0;

  virtual ~GrapheneSender() = default;

  static std::unique_ptr<GrapheneSender> New(Dependency<::ArgsManager>,
                                             Dependency<TxPool>);
};

}  // namespace p2p

#endif  //UNITE_P2P_GRAPHENE_SENDER_H
