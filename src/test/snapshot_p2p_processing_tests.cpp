// Copyright (c) 2018 The unit-e core developers
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
#include <snapshot/state.h>
#include <test/test_unite.h>
#include <validation.h>
#include <version.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_p2p_processing_tests, TestingSetup)

std::unique_ptr<CNode> mockNode() {
  uint32_t ip = 0xa0b0c001;
  in_addr s{ip};
  CService service(CNetAddr(s), 8333);
  CAddress addr(service, NODE_NONE);

  auto node = MakeUnique<CNode>(0, ServiceFlags(NODE_NETWORK | NODE_WITNESS), 0,
                                INVALID_SOCKET, addr, 0, 0, CAddress(),
                                "", /*fInboundIn=*/
                                false);
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

BOOST_AUTO_TEST_CASE(snapshot_process_p2p_snapshot_sequentially) {
  SetDataDir("snapshot_process_p2p");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);
  snapshot::StoreCandidateBlockHash(uint256());

  CNetMsgMaker msgMaker(1);
  std::unique_ptr<CNode> node(mockNode());

  uint256 bestBlockHash = uint256S("aa");
  uint256 snapshotHash = uint256S(
      "920d33e3b53521c00827e998cbe2f63161f96cd1cd07e698eda393dffff0c0fe");
  const uint64_t totalMessages = 6;

  // simulate that header was already received
  mapBlockIndex[bestBlockHash] = new CBlockIndex;

  for (uint64_t i = 0; i < totalMessages / 2; ++i) {
    // simulate receiving the snapshot response
    snapshot::Snapshot snap;
    snapshot::UTXOSubset subset1;
    snapshot::UTXOSubset subset2;
    subset1.m_txId = uint256FromUint64(i * 2);
    subset2.m_txId = uint256FromUint64(i * 2 + 1);
    subset1.m_outputs[0] = CTxOut();
    subset2.m_outputs[0] = CTxOut();
    snap.m_utxoSubsets.emplace_back(subset1);
    snap.m_utxoSubsets.emplace_back(subset2);
    snap.m_snapshotHash = snapshotHash;
    snap.m_bestBlockHash = bestBlockHash;
    snap.m_utxoSubsetIndex = i * 2;
    snap.m_totalUTXOSubsets = totalMessages;

    CDataStream body(SER_NETWORK, PROTOCOL_VERSION);
    body << snap;
    BOOST_CHECK_MESSAGE(snapshot::ProcessSnapshot(node.get(), body, msgMaker),
                        "failed to process snapshot message on m_step="
                            << i << ". probably snapshot hash is incorrect");

    if (i < (totalMessages / 2 - 1)) {  // ask the peer for more messages
      BOOST_CHECK_EQUAL(node->vSendMsg.size(), 2);  // header + body
      CMessageHeader header(Params().MessageStart());
      CDataStream(node->vSendMsg[0], SER_NETWORK, PROTOCOL_VERSION) >> header;
      BOOST_CHECK_EQUAL(header.GetCommand(), "getsnapshot");

      snapshot::GetSnapshot get;
      CDataStream(node->vSendMsg[1], SER_NETWORK, PROTOCOL_VERSION) >> get;
      BOOST_CHECK_EQUAL(get.m_bestBlockHash.GetHex(),
                        snap.m_bestBlockHash.GetHex());

      uint64_t expSize = snap.m_utxoSubsets.size() + (i * 2);
      BOOST_CHECK_EQUAL(get.m_utxoSubsetIndex, expSize);
      BOOST_CHECK(get.m_utxoSubsetCount == snapshot::MAX_UTXO_SET_COUNT);
      node->vSendMsg.clear();
    } else {  // finish snapshot downloading
      BOOST_CHECK(node->vSendMsg.empty());
    }
  }

  uint32_t snapId{0};
  BOOST_CHECK(pcoinsdbview->GetCandidateSnapshotId(snapId));
  std::unique_ptr<snapshot::Indexer> idx(snapshot::Indexer::Open(snapId));
  BOOST_CHECK(idx->GetMeta().m_bestBlockHash == bestBlockHash);
  BOOST_CHECK(idx->GetMeta().m_totalUTXOSubsets == totalMessages);

  uint64_t i{0};
  snapshot::Iterator iter(std::move(idx));
  while (iter.Valid()) {
    BOOST_CHECK(iter.GetUTXOSubset().m_txId.GetUint64(0) == i);
    ++i;
    iter.Next();
  }
  BOOST_CHECK(totalMessages == i);
}

BOOST_AUTO_TEST_CASE(snapshot_process_p2p_snapshot_switch_height) {
  SetDataDir("snapshot_process_p2p");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);
  snapshot::StoreCandidateBlockHash(uint256());

  // chain of 1 -> 2 -> 3 blocks
  auto *bi1 = new CBlockIndex;
  bi1->nHeight = 1;
  auto *bi2 = new CBlockIndex;
  bi2->nHeight = 2;
  bi2->pprev = bi1;
  auto *bi3 = new CBlockIndex;
  bi3->nHeight = 3;
  bi3->pprev = bi2;

  auto pair = mapBlockIndex.emplace(uint256S("aa"), bi1).first;
  bi1->phashBlock = &pair->first;
  pair = mapBlockIndex.emplace(uint256S("bb"), bi2).first;
  bi2->phashBlock = &pair->first;
  pair = mapBlockIndex.emplace(uint256S("cc"), bi3).first;
  bi3->phashBlock = &pair->first;

  CNetMsgMaker msgMaker(1);
  std::unique_ptr<CNode> node(mockNode());

  snapshot::Snapshot snap;
  snap.m_utxoSubsets.emplace_back(snapshot::UTXOSubset());
  snap.m_bestBlockHash = bi1->GetBlockHash();
  snap.m_utxoSubsetIndex = 0;
  snap.m_totalUTXOSubsets = 5;
  CDataStream body(SER_NETWORK, PROTOCOL_VERSION);
  body << snap;

  // process first chunk and ask for the next one
  BOOST_CHECK(snapshot::ProcessSnapshot(node.get(), body, msgMaker));
  uint32_t snapId{0};
  BOOST_CHECK(pcoinsdbview->GetInitSnapshotId(snapId));
  BOOST_CHECK_EQUAL(snapId, 0);
  snapshot::GetSnapshot get;
  CDataStream(node->vSendMsg[1], SER_NETWORK, PROTOCOL_VERSION) >> get;
  BOOST_CHECK_EQUAL(get.m_bestBlockHash, bi1->GetBlockHash());
  BOOST_CHECK_EQUAL(get.m_utxoSubsetIndex, 1);
  node->vSendMsg.clear();

  // switch to higher block height and ask for the next chunk
  snap.m_bestBlockHash = bi3->GetBlockHash();
  body << snap;

  BOOST_CHECK(snapshot::ProcessSnapshot(node.get(), body, msgMaker));
  BOOST_CHECK(pcoinsdbview->GetInitSnapshotId(snapId));
  BOOST_CHECK_EQUAL(snapId, 1);
  CDataStream(node->vSendMsg[1], SER_NETWORK, PROTOCOL_VERSION) >> get;
  BOOST_CHECK_EQUAL(get.m_bestBlockHash, bi3->GetBlockHash());
  BOOST_CHECK_EQUAL(get.m_utxoSubsetIndex, 1);
  node->vSendMsg.clear();

  // don't switch to lower block height but ask the peer if it has the next
  // chunk of our snapshot
  snap.m_bestBlockHash = bi2->GetBlockHash();
  body << snap;
  BOOST_CHECK(snapshot::ProcessSnapshot(node.get(), body, msgMaker));
  BOOST_CHECK(pcoinsdbview->GetInitSnapshotId(snapId));
  BOOST_CHECK_EQUAL(snapId, 1);
  CDataStream(node->vSendMsg[1], SER_NETWORK, PROTOCOL_VERSION) >> get;
  BOOST_CHECK_EQUAL(get.m_bestBlockHash, bi3->GetBlockHash());
  BOOST_CHECK_EQUAL(get.m_utxoSubsetIndex, 1);
  node->vSendMsg.clear();
}

BOOST_AUTO_TEST_CASE(snapshot_start_initial_snapshot_download) {
  snapshot::EnableISDMode();
  snapshot::StoreCandidateBlockHash(uint256());
  snapshot::HeadersDownloaded();

  CNetMsgMaker msgMaker(1);
  std::unique_ptr<CNode> node(mockNode());
  snapshot::StartInitialSnapshotDownload(node.get(), msgMaker);
  BOOST_CHECK_EQUAL(node->vSendMsg.size(), 2);

  CMessageHeader header(Params().MessageStart());
  CDataStream(node->vSendMsg[0], SER_NETWORK, PROTOCOL_VERSION) >> header;
  BOOST_CHECK(header.GetCommand() == "getsnapshot");

  snapshot::GetSnapshot get;
  CDataStream(node->vSendMsg[1], SER_NETWORK, PROTOCOL_VERSION) >> get;
  BOOST_CHECK(get.m_bestBlockHash.IsNull());
  BOOST_CHECK_EQUAL(get.m_utxoSubsetIndex, 0);
  BOOST_CHECK_EQUAL(get.m_utxoSubsetCount, snapshot::MAX_UTXO_SET_COUNT);
  BOOST_CHECK(node->sentInitGetSnapshot);

  node->vSendMsg.clear();
  snapshot::StartInitialSnapshotDownload(node.get(), msgMaker);
  BOOST_CHECK(node->vSendMsg.empty());
}

BOOST_AUTO_TEST_CASE(snapshot_find_next_blocks_to_download) {
  snapshot::EnableISDMode();

  // return 0 blocks as we have not received the parent header of the snapshot
  const auto candidate = std::make_pair(uint256S("aa"), new CBlockIndex);
  auto record1 = mapBlockIndex.insert(candidate).first;
  record1->second->phashBlock = &record1->first;
  snapshot::StoreCandidateBlockHash(candidate.first);

  std::vector<const CBlockIndex *> blocks;
  BOOST_CHECK(snapshot::FindNextBlocksToDownload(0, blocks));
  BOOST_CHECK(blocks.empty());

  // return the parent blockIndex of the snapshot to download
  const auto parent = std::make_pair(uint256S("bb"), new CBlockIndex);
  auto record2 = mapBlockIndex.insert(parent).first;
  record2->second->phashBlock = &record2->first;
  record2->second->pprev = record1->second;

  BOOST_CHECK(snapshot::FindNextBlocksToDownload(0, blocks));
  BOOST_CHECK_EQUAL(blocks.size(), 1);
  BOOST_CHECK_EQUAL(blocks[0]->GetBlockHash().GetHex(), parent.first.GetHex());
}

BOOST_AUTO_TEST_SUITE_END()
