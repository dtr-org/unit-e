// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_SNAPSHOT_P2P_PROCESSING_H
#define UNITE_SNAPSHOT_P2P_PROCESSING_H

#include <snapshot/params.h>

#include <stdint.h>
#include <chrono>
#include <memory>
#include <vector>

#include <chain.h>
#include <net.h>
#include <netmessagemaker.h>
#include <snapshot/indexer.h>
#include <snapshot/messages.h>
#include <streams.h>
#include <uint256.h>

namespace snapshot {

constexpr uint16_t MAX_UTXO_SET_COUNT = 10000;

class P2PState {
  using steady_clock = std::chrono::steady_clock;
  using time_point = steady_clock::time_point;

 public:
  explicit P2PState(const Params &params = Params());

  //! sends to the node the header of the best snapshot
  bool ProcessGetSnapshotHeader(CNode &node, CDataStream &data,
                                const CNetMsgMaker &msg_maker);

  //! saves node's best snapshot
  bool ProcessSnapshotHeader(CNode &node, CDataStream &data);

  //! sends to node the requested chunk
  bool ProcessGetSnapshot(CNode &node, CDataStream &data,
                          const CNetMsgMaker &msg_maker);

  //! saves the received snapshot chunk.
  //! if it wasn't the last chunk, request the following one
  //! if it was the last chunk, finishes snapshot downloading processed
  bool ProcessSnapshot(CNode &node, CDataStream &data,
                       const CNetMsgMaker &msg_maker);

  //! requests the snapshot from the node if it has the best one
  //! can request the second best snapshot if previous one was detected broken
  void StartInitialSnapshotDownload(CNode &node, int node_index, int total_nodes,
                                    const CNetMsgMaker &msg_maker);

  //! Invokes inside original FindNextBlocksToDownload and returns the block
  //! which is the best block of the candidate snapshot. Returns true if the
  //! original function should be terminated
  bool FindNextBlocksToDownload(NodeId node_id,
                                std::vector<const CBlockIndex *> &blocks);

  void ProcessSnapshotParentBlock(CBlock *parent_block,
                                  std::function<void()> regular_processing);

 protected:
  // used to detect the timeout after which node gives up
  // and switching to IBD
  time_point m_first_discovery_request_at = time_point::min();

  // snapshot that node decided to download
  // and was the best one at a time the decision was made
  SnapshotHeader m_downloading_snapshot;

 private:
  Params m_params;

  // keeps track of the best snapshot across all peers
  SnapshotHeader m_best_snapshot;

  bool SendGetSnapshot(CNode &node, GetSnapshot &msg,
                       const CNetMsgMaker &msg_maker);

  //! returns node's best_snapshot if it points to finalized epoch
  //! and downloading process hasn't timed out
  SnapshotHeader NodeBestSnapshot(CNode &node);

  //! update m_best_snapshot if provided one is a better one
  void SetIfBestSnapshot(const SnapshotHeader &best_snapshot);
};

void InitP2P(const Params &params);

// proxy to g_p2p_state.ProcessGetSnapshotHeader
bool ProcessGetBestSnapshot(CNode &node, CDataStream &data,
                            const CNetMsgMaker &msg_maker);
// proxy to g_p2p_state.ProcessSnapshotHeader
bool ProcessBestSnapshot(CNode &node, CDataStream &data);

// proxy to g_p2p_state.ProcessGetSnapshot
bool ProcessGetSnapshot(CNode &node, CDataStream &data,
                        const CNetMsgMaker &msg_maker);

// proxy to g_p2p_state.ProcessSnapshot
bool ProcessSnapshot(CNode &node, CDataStream &data,
                     const CNetMsgMaker &msg_maker);

// proxy to g_p2p_state.StartInitialSnapshotDownload
void StartInitialSnapshotDownload(CNode &node, int node_index, int total_nodes,
                                  const CNetMsgMaker &msg_maker);

// proxy to g_p2p_state.FindNextBlocksToDownload
bool FindNextBlocksToDownload(NodeId node_id,
                              std::vector<const CBlockIndex *> &blocks);

// proxy to g_p2p_state.ProcessSnapshotParentBlock
void ProcessSnapshotParentBlock(CBlock *parent_block,
                                std::function<void()> regular_processing);
}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_P2P_PROCESSING_H
