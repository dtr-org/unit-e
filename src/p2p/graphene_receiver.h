// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_GRAPHENE_RECEIVER_H
#define UNITE_P2P_GRAPHENE_RECEIVER_H

#include <unordered_map>

#include <chain.h>
#include <dependency.h>
#include <net.h>
#include <netmessagemaker.h>
#include <p2p/graphene.h>
#include <protocol.h>

namespace p2p {

class GrapheneReceiver {
 public:
  //! \brief Requests graphene block(s) if certain conditions are met
  //!
  //! \param last_inv_block_index - CBlockIndex of the last block we are requesting
  //! \param blocks_in_flight - how many blocks are currently in flight. This
  //! should include \p invs_in_out
  //! \param invs - invs that we are going to request from \p from
  //! \returns true if graphene blocks were requested
  virtual bool RequestBlocks(CNode &from,
                             const CBlockIndex &last_inv_block_index,
                             size_t blocks_in_flight,
                             const std::vector<CInv> &invs) = 0;

  virtual void OnGrapheneBlockReceived(CNode &from, const GrapheneBlock &graphene_block) = 0;

  virtual void OnGrapheneTxReceived(CNode &from, const GrapheneTx &graphene_tx) = 0;

  virtual void OnDisconnected(NodeId node) = 0;

  virtual void OnMarkedAsReceived(NodeId node, const uint256 &block_hash) = 0;

  virtual ~GrapheneReceiver() = default;

  static std::unique_ptr<GrapheneReceiver> New(Dependency<::ArgsManager>,
                                               Dependency<::TxPool> txpool);
};

}  // namespace p2p

#endif  //UNITE_P2P_GRAPHENE_RECEIVER_H
