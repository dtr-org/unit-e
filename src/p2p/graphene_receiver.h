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
  explicit GrapheneReceiver(bool enabled,
                            Dependency<TxPool> txpool);

  //! \brief Requests graphene blocks if applicable
  //! \param invs - invs that we are going to request from \param from
  //! \param last_inv_block_index - CBlockIndex of the last block we are requesting
  //! \param blocks_in_flight - how many blocks are currently in flight. This
  //! should include \param invs
  //!
  //! This function checks invs that are being requested and if conditions for
  //! graphene met - sends corresponding graphene requests, removing corresponding invs
  void BeforeBlocksRequested(CNode &from,
                             std::vector<CInv> &invs,
                             const CBlockIndex &last_inv_block_index,
                             size_t blocks_in_flight);

  void OnGrapheneBlockReceived(CNode &from,
                               const GrapheneBlock &graphene_block);

  void OnGrapheneTxReceived(CNode &from, const GrapheneTx &graphene_tx);

  void OnDisconnected(NodeId node);

  static std::unique_ptr<GrapheneReceiver> New(Dependency<::ArgsManager>,
                                               Dependency<::TxPool> txpool);

 private:
  struct BlockReceiveState {
    BlockReceiveState(const uint256 &block_hash, NodeId sender)
        : block_hash(block_hash), sender(sender) {}
    uint256 block_hash;
    NodeId sender;
    std::unique_ptr<GrapheneBlockReconstructor> reconstructor;
  };

  const bool m_enabled;
  CCriticalSection m_cs;
  Dependency<TxPool> m_txpool;

  // Currently we can only download one graphene block at a time,
  // used map mostly for future where we might reconsider this
  std::map<uint256, BlockReceiveState> m_graphene_blocks_in_flight GUARDED_BY(m_cs);

  void RequestFallbackBlock(CNode &from, const uint256 &block_hash);

  bool CheckMerkleRoot(const CBlock &block);

  void ReconstructAndSubmitBlock(CNode &from,
                                 const GrapheneBlockReconstructor &reconstructor);

  void MarkBlockNotInFlight(const CNode &from, const uint256 &block_hash);

  bool IsInFlight(const uint256 &block_hash, NodeId sender);
};

}  // namespace p2p

#endif  //UNITE_P2P_GRAPHENE_RECEIVER_H
