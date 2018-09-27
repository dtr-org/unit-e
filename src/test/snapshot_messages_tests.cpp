// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/messages.h>

#include <test/test_unite.h>
#include <utilstrencodings.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_p2p_messages_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(snapshot_utx_serializer) {
  CDataStream s(SER_NETWORK, INIT_PROTO_VERSION);
  auto utxoSet = snapshot::UTXOSet();
  s << utxoSet;

  std::string exp;
  exp =
      "00000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "00000000"                          // height
      "00"                                // isCoinBase
      "00";                               // outputs
  BOOST_CHECK_EQUAL(HexStr(s), exp);
  s.clear();

  utxoSet.m_txId.SetHex("aa");
  utxoSet.m_height = 0xbb;
  utxoSet.m_isCoinBase = true;
  s << utxoSet;
  exp =
      "aa000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "bb000000"                          // tx height
      "01"                                // isCoinBase
      "00";                               // outputs
  BOOST_CHECK_MESSAGE(HexStr(s) == exp,
                      "expected: " << HexStr(s) << " got: " << exp);
  s.clear();

  utxoSet.m_outputs[2] = CTxOut();
  s << utxoSet;
  exp =
      "aa000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "bb000000"                          // tx height
      "01"                                // isCoinBase
      "01"                                // outputs count
      "02000000"                          // outpoint index
      "ffffffffffffffff"                  // nValue (-1 by default)
      "00";                               // scriptPubKey length
  BOOST_CHECK_EQUAL(HexStr(s), exp);
  s.clear();

  auto out = CTxOut();
  out.nValue = 0xcc;
  out.scriptPubKey << OP_RETURN;
  utxoSet.m_outputs[2] = out;
  s << utxoSet;
  exp =
      "aa000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "bb000000"                          // tx height
      "01"                                // isCoinBase
      "01"                                // outputs count
      "02000000"                          // outpoint index
      "cc00000000000000"                  // nValue (-1 by default)
      "01"                                // scriptPubKey length
      "6a";                               // script data
  BOOST_CHECK_EQUAL(HexStr(s), exp);
  s.clear();
}

BOOST_AUTO_TEST_CASE(snapshot_p2p_get_snapshot_serialization) {
  snapshot::GetSnapshot msg;
  msg.m_bestBlockHash.SetHex("bb");
  msg.m_utxoSetIndex = 55;
  msg.m_utxoSetCount = 17;

  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << msg;
  BOOST_CHECK_MESSAGE(stream.size() == 42,
                      "size expected: " << 42 << " got: " << stream.size());

  std::string got = HexStr(stream);
  std::string exp =
      "bb000000000000000000000000000000"  // block hash
      "00000000000000000000000000000000"  //
      "3700000000000000"                  // index
      "1100";                             // length
  BOOST_CHECK_MESSAGE(got == exp, "expected: " << exp << " got: " << got);

  snapshot::GetSnapshot msg2;
  stream >> msg2;
  BOOST_CHECK_EQUAL(msg.m_bestBlockHash, msg2.m_bestBlockHash);
  BOOST_CHECK_EQUAL(msg.m_utxoSetIndex, msg2.m_utxoSetIndex);
  BOOST_CHECK_EQUAL(msg.m_utxoSetCount, msg2.m_utxoSetCount);
}

BOOST_AUTO_TEST_CASE(snapshot_p2p_snapshot_serialization) {
  // serialize empty message
  snapshot::Snapshot msg;
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << msg;
  BOOST_CHECK_EQUAL(stream.size(), 81);

  std::string got = HexStr(stream);
  std::string exp =
      "00000000000000000000000000000000"  // snapshot hash
      "00000000000000000000000000000000"  //
      "00000000000000000000000000000000"  // block hash
      "00000000000000000000000000000000"  //
      "0000000000000000"                  // total utxo sets
      "0000000000000000"                  // utxo set index
      "00";                               // utxo set count
  BOOST_CHECK_EQUAL(got, exp);

  // serialize filled
  msg.m_snapshotHash.SetHex("aa");
  msg.m_bestBlockHash.SetHex("bb");
  msg.m_totalUTXOSets = 25000000;
  msg.m_utxoSetIndex = 128;

  snapshot::UTXOSet utxoSet;
  utxoSet.m_height = 53;
  utxoSet.m_isCoinBase = true;
  utxoSet.m_txId.SetHex("bb");
  CScript script;
  script << OP_RETURN;
  utxoSet.m_outputs[5] = CTxOut(5, script);
  msg.m_utxoSets.emplace_back(utxoSet);

  stream.clear();
  stream << msg;
  BOOST_CHECK_EQUAL(stream.size(), 133);

  got = HexStr(stream);
  exp =
      "aa000000000000000000000000000000"  // snapshot hash
      "00000000000000000000000000000000"  //
      "bb000000000000000000000000000000"  // block hash
      "00000000000000000000000000000000"  //
      "40787d0100000000"                  // total utxo sets
      "8000000000000000"                  // index
      "01"                                // utxo set count
      "bb000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "35000000"                          // tx height
      "01"                                // isCoinBase
      "01"                                // output size
      "05000000"                          // output index
      "0500000000000000"                  // amount
      "01"                                // script size
      "6a";                               // script
  BOOST_CHECK_EQUAL(got, exp);

  snapshot::Snapshot msg2;
  stream >> msg2;
  BOOST_CHECK_EQUAL(msg.m_bestBlockHash, msg2.m_bestBlockHash);
  BOOST_CHECK_EQUAL(msg.m_totalUTXOSets, msg2.m_totalUTXOSets);
  BOOST_CHECK_EQUAL(msg.m_utxoSetIndex, msg2.m_utxoSetIndex);
  BOOST_CHECK_EQUAL(msg.m_utxoSets.size(), msg2.m_utxoSets.size());
  BOOST_CHECK_EQUAL(msg.m_utxoSets[0].m_txId, msg2.m_utxoSets[0].m_txId);
  BOOST_CHECK_EQUAL(msg.m_utxoSets[0].m_outputs.size(),
                    msg2.m_utxoSets[0].m_outputs.size());
}

BOOST_AUTO_TEST_SUITE_END()
