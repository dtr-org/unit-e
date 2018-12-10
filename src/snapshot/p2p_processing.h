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
 public:
  explicit P2PState(const Params &params = Params());

  bool ProcessGetSnapshot(CNode *node, CDataStream &data,
                          const CNetMsgMaker &msg_maker);

  //! Saves the current snapshot batch on the disk and
  //! asks remaining messages from the peer.
  //!
  //! Scenario 1. current node doesn't have any part of the initial snapshot.
  //! rules:
  //! 1. if peer sends the batch with the message index 0 (first batch) then
  //! the current node saves it and asks the peer for remaining batches.
  //! 2. if peer sends the batch which has index > 0 (not the first one) then
  //! the current node ignores this batch and asks the peer to send the first
  //! batch of the same snapshot.
  //!
  //! Scenario 2. current node has 1+ messages from the initial snapshot.
  //! rules:
  //! 1. if peer sends the next batch of the same snapshot, the current node
  //! saves it on the disk and re-ask remaining messages if any left.
  //! 2. if the peer sends the batch which is not the next one of the initial
  //! snapshot the current node has, then the current node ignores this batch
  //! and re-asks the peer for the right. utxIndex=messages.size()
  //! 3. if the peer sends the batch with the lower block height than the one
  //! the current node has, this batch is ignored
  //! 4. if the peer sends the batch with the higher block height than
  //! the current node has, the current node resets the initial snapshot
  //! and processes the batch according to rules in Scenario 1.
  //! 5. if the peer sends the block hash which doesn't match with the hash of
  //! the initial snapshot, this batch is ignored
  bool ProcessSnapshot(CNode *node, CDataStream &data,
                       const CNetMsgMaker &msg_maker);

  //! Starts the initial snapshot download if needed
  void StartInitialSnapshotDownload(CNode *node, const CNetMsgMaker &msg_maker);

  //! Invokes inside original FindNextBlocksToDownload and returns the block
  //! which is the best block of the candidate snapshot. Returns true if the
  //! original function should be terminated
  bool FindNextBlocksToDownload(NodeId node_id,
                                std::vector<const CBlockIndex *> &blocks);

  void ProcessSnapshotParentBlock(CBlock *parent_block,
                                  std::function<void()> regular_processing);

 private:
  Params m_params;

  // keep track of fast sync requests
  std::chrono::steady_clock::time_point m_first_request_at;
  std::chrono::steady_clock::time_point m_last_request_at;

  bool SendGetSnapshot(CNode *node, GetSnapshot &msg,
                       const CNetMsgMaker &msg_maker);
  bool SaveSnapshotAndRequestMore(std::unique_ptr<Indexer> indexer,
                                  Snapshot &snap, CNode *node,
                                  const CNetMsgMaker &msg_maker);
};

void InitP2P(const Params &params);

// proxy to g_p2p_state.ProcessGetSnapshot
bool ProcessGetSnapshot(CNode *node, CDataStream &data,
                        const CNetMsgMaker &msg_maker);

// proxy to g_p2p_state.ProcessSnapshot
bool ProcessSnapshot(CNode *node, CDataStream &data,
                     const CNetMsgMaker &msg_maker);

// proxy to g_p2p_state.StartInitialSnapshotDownload
void StartInitialSnapshotDownload(CNode *node, const CNetMsgMaker &msg_maker);

// proxy to g_p2p_state.FindNextBlocksToDownload
bool FindNextBlocksToDownload(NodeId node_id,
                              std::vector<const CBlockIndex *> &blocks);

// proxy to g_p2p_state.ProcessSnapshotParentBlock
void ProcessSnapshotParentBlock(CBlock *parent_block,
                                std::function<void()> regular_processing);
}  // namespace snapshot

#endif  // UNITE_SNAPSHOT_P2P_PROCESSING_H
