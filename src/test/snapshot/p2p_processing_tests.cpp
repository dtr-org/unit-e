// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/p2p_processing.h>

#include <memory>

#include <chainparams.h>
#include <net.h>
#include <netaddress.h>
#include <serialize.h>
#include <snapshot/indexer.h>
#include <snapshot/iterator.h>
#include <snapshot/messages.h>
#include <snapshot/snapshot_index.h>
#include <snapshot/state.h>
#include <test/test_unite.h>
#include <validation.h>
#include <version.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_p2p_processing_tests, TestingSetup)

bool HasSnapshotHash(const uint256 &hash) {
  for (const snapshot::Checkpoint &p : snapshot::GetSnapshotCheckpoints()) {
    if (p.snapshot_hash == hash) {
      return true;
    }
  }
  return false;
}

class MockP2PState : public snapshot::P2PState {
 public:
  explicit MockP2PState(const snapshot::Params &params = snapshot::Params())
      : P2PState(params) {}

  void MockBestSnapshot(const snapshot::SnapshotHeader best_snapshot) {
    m_downloading_snapshot = best_snapshot;
  }

  void MockFirstDiscoveryRequestAt(const std::chrono::steady_clock::time_point &time) {
    m_first_discovery_request_at = time;
  }
};

std::unique_ptr<CNode> MockNode() {
  uint32_t ip = 0xa0b0c001;
  in_addr s{ip};
  CService service(CNetAddr(s), 7182);
  CAddress addr(service, NODE_NONE);

  auto node = MakeUnique<CNode>(0, ServiceFlags(NODE_NETWORK | NODE_WITNESS), 0,
                                INVALID_SOCKET, addr, 0, 0, CAddress(),
                                "", /*fInboundIn=*/
                                false);
  node->nServices = ServiceFlags(NODE_NETWORK | NODE_WITNESS | NODE_SNAPSHOT);
  node->nVersion = 1;
  node->fSuccessfullyConnected = true;
  return node;
}

uint256 uint256FromUint64(uint64_t n) {
  CDataStream s(SER_DISK, PROTOCOL_VERSION);
  s << n;
  s << uint64_t(0);
  s << uint64_t(0);
  s << uint64_t(0);
  uint256 nn;
  s >> nn;
  return nn;
}

BOOST_AUTO_TEST_CASE(process_snapshot) {
  SetDataDir("snapshot_process_p2p");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);
  snapshot::StoreCandidateBlockHash(uint256());
  snapshot::EnableISDMode();
  MockP2PState p2p_state;

  CNetMsgMaker msg_maker(1);
  std::unique_ptr<CNode> node(MockNode());

  LOCK2(cs_main, node->cs_vSend);

  snapshot::SnapshotHeader best_snapshot;
  best_snapshot.snapshot_hash = uint256S("294f4fba05bc2f19764960989b4a364466522b3009808ff99e89cfde56bf43e7");
  best_snapshot.block_hash = uint256S("aa");
  best_snapshot.stake_modifier = uint256S("bb");
  best_snapshot.chain_work = uint256S("cc");
  best_snapshot.total_utxo_subsets = 6;

  node->m_best_snapshot = best_snapshot;
  p2p_state.MockBestSnapshot(best_snapshot);

  // simulate that headers were already received
  auto bi = new CBlockIndex;
  bi->stake_modifier = best_snapshot.stake_modifier;
  bi->phashBlock = &mapBlockIndex.emplace(best_snapshot.block_hash, bi).first->first;

  for (uint64_t i = 0; i < best_snapshot.total_utxo_subsets / 2; ++i) {
    // simulate receiving the snapshot response
    snapshot::Snapshot snap;
    snapshot::UTXOSubset subset1;
    snapshot::UTXOSubset subset2;
    subset1.tx_id = uint256FromUint64(i * 2);
    subset2.tx_id = uint256FromUint64(i * 2 + 1);
    subset1.outputs[0] = CTxOut();
    subset2.outputs[0] = CTxOut();
    snap.utxo_subsets.emplace_back(subset1);
    snap.utxo_subsets.emplace_back(subset2);
    snap.snapshot_hash = best_snapshot.snapshot_hash;
    snap.utxo_subset_index = i * 2;

    CDataStream body(SER_NETWORK, PROTOCOL_VERSION);
    body << snap;
    BOOST_CHECK_MESSAGE(p2p_state.ProcessSnapshot(*node, body, msg_maker),
                        "failed to process snapshot message on m_step="
                            << i << ". probably snapshot hash is incorrect");

    if (i < (best_snapshot.total_utxo_subsets / 2 - 1)) {  // ask the peer for more messages
      BOOST_CHECK_EQUAL(node->vSendMsg.size(), 2);         // header + body
      CMessageHeader header(Params().MessageStart());
      CDataStream(node->vSendMsg[0], SER_NETWORK, PROTOCOL_VERSION) >> header;
      BOOST_CHECK_EQUAL(header.GetCommand(), "getsnapshot");

      snapshot::GetSnapshot get;
      CDataStream(node->vSendMsg[1], SER_NETWORK, PROTOCOL_VERSION) >> get;
      BOOST_CHECK_EQUAL(get.snapshot_hash.GetHex(), best_snapshot.snapshot_hash.GetHex());

      uint64_t expSize = snap.utxo_subsets.size() + (i * 2);
      BOOST_CHECK_EQUAL(get.utxo_subset_index, expSize);
      BOOST_CHECK(get.utxo_subset_count == snapshot::MAX_UTXO_SET_COUNT);
      node->vSendMsg.clear();
    } else {  // finish snapshot downloading
      BOOST_CHECK(node->vSendMsg.empty());
    }
  }

  // test that snapshot was created
  LOCK(snapshot::cs_snapshot);
  BOOST_CHECK(HasSnapshotHash(best_snapshot.snapshot_hash));
  std::unique_ptr<snapshot::Indexer> idx(snapshot::Indexer::Open(best_snapshot.snapshot_hash));
  const snapshot::SnapshotHeader &snapshot_header = idx->GetSnapshotHeader();
  BOOST_CHECK_EQUAL(snapshot_header.snapshot_hash.GetHex(), best_snapshot.snapshot_hash.GetHex());
  BOOST_CHECK_EQUAL(snapshot_header.block_hash.GetHex(), best_snapshot.block_hash.GetHex());
  BOOST_CHECK_EQUAL(snapshot_header.stake_modifier.GetHex(), best_snapshot.stake_modifier.GetHex());
  BOOST_CHECK_EQUAL(snapshot_header.total_utxo_subsets, best_snapshot.total_utxo_subsets);

  // test that snapshot has correct content
  uint64_t total = 0;
  snapshot::Iterator iter(std::move(idx));
  while (iter.Valid()) {
    BOOST_CHECK(iter.GetUTXOSubset().tx_id.GetUint64(0) == total);
    ++total;
    iter.Next();
  }
  BOOST_CHECK_EQUAL(best_snapshot.total_utxo_subsets, total);
}

BOOST_AUTO_TEST_CASE(start_initial_snapshot_download) {
  snapshot::InitP2P(Params().GetSnapshotParams());
  snapshot::EnableISDMode();
  snapshot::StoreCandidateBlockHash(uint256());
  MockP2PState p2p_state(Params().GetSnapshotParams());

  LOCK(cs_main);

  auto *b1 = new CBlockIndex;
  auto *b2 = new CBlockIndex;
  b1->phashBlock = &mapBlockIndex.emplace(uint256S("aa"), b1).first->first;
  b2->phashBlock = &mapBlockIndex.emplace(uint256S("bb"), b2).first->first;
  b2->pprev = b1;
  b1->nHeight = 1;
  b2->nHeight = 2;

  snapshot::SnapshotHeader best;
  best.snapshot_hash = uint256S("a2");
  best.block_hash = b2->GetBlockHash();

  snapshot::SnapshotHeader second_best;
  second_best.snapshot_hash = uint256S("a1");
  second_best.block_hash = b1->GetBlockHash();

  std::unique_ptr<CNode> node1(MockNode());  // no snapshot
  std::unique_ptr<CNode> node2(MockNode());  // second best
  std::unique_ptr<CNode> node3(MockNode());  // best
  std::unique_ptr<CNode> node4(MockNode());  // best
  std::vector<CNode *> nodes{node1.get(), node2.get(), node3.get(), node4.get()};

  LOCK(node1->cs_vSend);
  LOCK(node2->cs_vSend);
  LOCK(node3->cs_vSend);
  LOCK(node4->cs_vSend);

  // test that discovery message was sent
  CNetMsgMaker msg_maker(1);
  CMessageHeader header(Params().MessageStart());
  for (size_t i = 0; i < nodes.size(); ++i) {
    CNode &node = *nodes[i];
    p2p_state.StartInitialSnapshotDownload(node, i, nodes.size(), msg_maker, *b2);
    BOOST_CHECK(node.m_snapshot_discovery_sent);
    BOOST_CHECK_EQUAL(node.vSendMsg.size(), 1);
    CDataStream(node.vSendMsg[0], SER_NETWORK, PROTOCOL_VERSION) >> header;
    BOOST_CHECK(header.GetCommand() == "getsnaphead");
    node.vSendMsg.clear();
  }

  // test that discovery message is sent once
  for (size_t i = 0; i < nodes.size(); ++i) {
    CNode &node = *nodes[i];
    p2p_state.StartInitialSnapshotDownload(node, i, nodes.size(), msg_maker, *b2);
    BOOST_CHECK(node.vSendMsg.empty());
  }

  {
    // mock that nodes without the snapshot timed out
    auto first_request_at = std::chrono::steady_clock::now();
    int64_t discovery_timeout_sec = Params().GetSnapshotParams().discovery_timeout_sec;
    first_request_at -= std::chrono::seconds(discovery_timeout_sec + 1);
    p2p_state.MockFirstDiscoveryRequestAt(first_request_at);
  }

  // mock headers that node can start detecting best snapshots
  snapshot::HeadersDownloaded();

  // node must detect the best snapshot during first loop
  node2->m_best_snapshot = second_best;
  node3->m_best_snapshot = best;
  node4->m_best_snapshot = best;
  for (size_t i = 0; i < nodes.size(); ++i) {
    CNode &node = *nodes[i];
    p2p_state.StartInitialSnapshotDownload(node, i, nodes.size(), msg_maker, *b2);
    BOOST_CHECK(node.vSendMsg.empty());
  }

  // test that node makes a request to peers with the best snapshot
  {
    auto now = std::chrono::steady_clock::now();

    for (size_t i = 0; i < nodes.size(); ++i) {
      CNode &node = *nodes[i];
      p2p_state.StartInitialSnapshotDownload(node, i, nodes.size(), msg_maker, *b2);
    }
    BOOST_CHECK(nodes[0]->vSendMsg.empty());
    BOOST_CHECK(nodes[1]->vSendMsg.empty());

    std::vector<CNode *> best_nodes{nodes[2], nodes[3]};
    for (CNode *node : best_nodes) {
      BOOST_CHECK(node->m_requested_snapshot_at >= now);
      BOOST_CHECK_EQUAL(node->vSendMsg.size(), 2);
      CDataStream(node->vSendMsg[0], SER_NETWORK, PROTOCOL_VERSION) >> header;
      BOOST_CHECK(header.GetCommand() == "getsnapshot");
      snapshot::GetSnapshot get;
      CDataStream(node->vSendMsg[1], SER_NETWORK, PROTOCOL_VERSION) >> get;
      BOOST_CHECK_EQUAL(get.snapshot_hash.GetHex(), best.snapshot_hash.GetHex());
      BOOST_CHECK_EQUAL(get.utxo_subset_index, 0);
      BOOST_CHECK_EQUAL(get.utxo_subset_count, 10000);

      node->vSendMsg.clear();
    }
  }

  // test that node fallbacks to second best snapshot
  // when peers with the best snapshot timed out
  {
    std::vector<CNode *> best_nodes{nodes[2], nodes[3]};
    for (CNode *n : best_nodes) {  // timeout one by one
      int64_t timeout = Params().GetSnapshotParams().snapshot_chunk_timeout_sec;
      n->m_requested_snapshot_at -= std::chrono::seconds(timeout + 1);
      for (size_t i = 0; i < nodes.size(); ++i) {
        CNode &node = *nodes[i];
        p2p_state.StartInitialSnapshotDownload(node, i, nodes.size(), msg_maker, *b2);
        BOOST_CHECK(node.vSendMsg.empty());
      }
    }

    // second best is requested
    for (size_t i = 0; i < nodes.size(); ++i) {
      CNode &node = *nodes[i];
      p2p_state.StartInitialSnapshotDownload(node, i, nodes.size(), msg_maker, *b2);
    }

    BOOST_CHECK(nodes[0]->vSendMsg.empty());
    BOOST_CHECK(nodes[2]->vSendMsg.empty());
    BOOST_CHECK(nodes[3]->vSendMsg.empty());

    BOOST_CHECK_EQUAL(nodes[1]->vSendMsg.size(), 2);
    CDataStream(nodes[1]->vSendMsg[0], SER_NETWORK, PROTOCOL_VERSION) >> header;
    BOOST_CHECK(header.GetCommand() == "getsnapshot");
    snapshot::GetSnapshot get;
    CDataStream(nodes[1]->vSendMsg[1], SER_NETWORK, PROTOCOL_VERSION) >> get;
    BOOST_CHECK_EQUAL(get.snapshot_hash.GetHex(), second_best.snapshot_hash.GetHex());
    BOOST_CHECK_EQUAL(get.utxo_subset_index, 0);
    BOOST_CHECK_EQUAL(get.utxo_subset_count, 10000);

    // restore state
    nodes[1]->vSendMsg.clear();
    nodes[1]->m_requested_snapshot_at = std::chrono::steady_clock::time_point::min();
    nodes[2]->m_requested_snapshot_at = std::chrono::steady_clock::now();
    nodes[3]->m_requested_snapshot_at = std::chrono::steady_clock::now();
    nodes[2]->m_best_snapshot = best;
    nodes[3]->m_best_snapshot = best;
    p2p_state.MockBestSnapshot(best);
  }

  // test that node fallbacks to second best snapshot
  // when peers with the best snapshot disconnected
  {
    for (size_t j = 1; j <= 2; ++j) {
      size_t total = nodes.size() - j;
      for (size_t i = 0; i < total; ++i) {  // disconnect one by one
        CNode &node = *nodes[i];
        p2p_state.StartInitialSnapshotDownload(node, i, total, msg_maker, *b2);
        BOOST_CHECK(node.vSendMsg.empty());
      }
    }

    // second best is requested
    for (size_t i = 0; i < nodes.size(); ++i) {
      CNode &node = *nodes[i];
      p2p_state.StartInitialSnapshotDownload(node, i, nodes.size(), msg_maker, *b2);
    }

    BOOST_CHECK(nodes[0]->vSendMsg.empty());
    BOOST_CHECK(nodes[2]->vSendMsg.empty());
    BOOST_CHECK(nodes[3]->vSendMsg.empty());

    BOOST_CHECK_EQUAL(nodes[1]->vSendMsg.size(), 2);
    CDataStream(nodes[1]->vSendMsg[0], SER_NETWORK, PROTOCOL_VERSION) >> header;
    BOOST_CHECK(header.GetCommand() == "getsnapshot");
    snapshot::GetSnapshot get;
    CDataStream(nodes[1]->vSendMsg[1], SER_NETWORK, PROTOCOL_VERSION) >> get;
    BOOST_CHECK_EQUAL(get.snapshot_hash.GetHex(), second_best.snapshot_hash.GetHex());
    BOOST_CHECK_EQUAL(get.utxo_subset_index, 0);
    BOOST_CHECK_EQUAL(get.utxo_subset_count, 10000);

    // restore state
    nodes[1]->vSendMsg.clear();
    nodes[1]->m_requested_snapshot_at = std::chrono::steady_clock::time_point::min();
    nodes[2]->m_requested_snapshot_at = std::chrono::steady_clock::now();
    nodes[3]->m_requested_snapshot_at = std::chrono::steady_clock::now();
    nodes[2]->m_best_snapshot = best;
    nodes[3]->m_best_snapshot = best;
    p2p_state.MockBestSnapshot(best);
  }

  // test that node does't disable ISD until timeout elapsed
  p2p_state.MockFirstDiscoveryRequestAt(std::chrono::steady_clock::now());
  p2p_state.StartInitialSnapshotDownload(*node1, 0, 1, msg_maker, *b2);
  BOOST_CHECK(snapshot::IsISDEnabled());

  // test that node disables ISD when there are no peers with the snapshot
  // and discovery timeout elapsed
  auto first_request_at = std::chrono::steady_clock::now();
  int64_t discovery_timeout_sec = Params().GetSnapshotParams().discovery_timeout_sec;
  first_request_at -= std::chrono::seconds(discovery_timeout_sec + 1);
  p2p_state.MockFirstDiscoveryRequestAt(first_request_at);
  p2p_state.StartInitialSnapshotDownload(*node1, 0, 1, msg_maker, *b2);
  BOOST_CHECK(!snapshot::IsISDEnabled());
}

BOOST_AUTO_TEST_CASE(snapshot_find_next_blocks_to_download) {
  snapshot::EnableISDMode();
  snapshot::P2PState p2p_state;

  LOCK(cs_main);

  // return 0 blocks as we have not received the parent header of the snapshot
  const auto candidate = std::make_pair(uint256S("aa"), new CBlockIndex);
  auto record1 = mapBlockIndex.insert(candidate).first;
  record1->second->phashBlock = &record1->first;
  snapshot::StoreCandidateBlockHash(candidate.first);

  std::vector<const CBlockIndex *> blocks;
  BOOST_CHECK(p2p_state.FindNextBlocksToDownload(0, blocks));
  BOOST_CHECK(blocks.empty());

  // return the parent blockIndex of the snapshot to download
  const auto parent = std::make_pair(uint256S("bb"), new CBlockIndex);
  auto record2 = mapBlockIndex.insert(parent).first;
  record2->second->phashBlock = &record2->first;
  record2->second->pprev = record1->second;

  BOOST_CHECK(p2p_state.FindNextBlocksToDownload(0, blocks));
  BOOST_CHECK_EQUAL(blocks.size(), 1);
  BOOST_CHECK_EQUAL(blocks[0]->GetBlockHash().GetHex(), parent.first.GetHex());
}

BOOST_AUTO_TEST_SUITE_END()
