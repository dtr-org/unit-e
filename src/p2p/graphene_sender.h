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
  GrapheneSender(bool enabled, Dependency<::TxPool> tx_pool);

  void UpdateRequesterTxPoolCount(const CNode &requester, uint64_t new_count);
  bool SendBlock(CNode &to, const CBlock &block, const CBlockIndex &index);
  void OnGrapheneTxRequestReceived(CNode &from,
                                   const GrapheneTxRequest &request);
  void OnDisconnected(NodeId node);

  static std::unique_ptr<GrapheneSender> New(Dependency<::ArgsManager>,
                                             Dependency<TxPool>);

 private:
  const bool m_enabled;

  struct ReceiverInfo {
    int last_requested_height = 0;
    uint256 last_requested_hash;
    bool requested_tx = false;
    uint64_t tx_pool_count = 0;
    uint64_t last_nonce = 0;
  };

  CCriticalSection m_cs;
  std::unordered_map<NodeId, ReceiverInfo> m_receiver_infos;
  Dependency<TxPool> m_sender_tx_pool;
  FastRandomContext m_random;
};

}  // namespace p2p

#endif  //UNITE_P2P_GRAPHENE_SENDER_H
