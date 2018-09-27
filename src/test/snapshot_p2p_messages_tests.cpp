// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/p2p_messages.h>

#include <test/test_unite.h>
#include <utilstrencodings.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_p2p_messages_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(snapshot_utx_serializer) {
  CDataStream s(SER_NETWORK, INIT_PROTO_VERSION);
  auto utx = snapshot::Utx();
  s << utx;

  std::string exp;
  exp =
      "00000000000000000000000000000000"  // hash
      "00000000000000000000000000000000"  //
      "00000000"                          // height
      "00"                                // isCoinBase
      "00";                               // outputs
  BOOST_CHECK_EQUAL(HexStr(s), exp);
  s.clear();

  utx.hash.SetHex("aa");
  utx.height = 0xbb;
  utx.isCoinBase = true;
  s << utx;
  exp =
      "aa000000000000000000000000000000"  // hash
      "00000000000000000000000000000000"  //
      "bb000000"                          // tx height
      "01"                                // isCoinBase
      "00";                               // outputs
  BOOST_CHECK_MESSAGE(HexStr(s) == exp,
                      "expected: " << HexStr(s) << " got: " << exp);
  s.clear();

  utx.outputs[2] = CTxOut();
  s << utx;
  exp =
      "aa000000000000000000000000000000"  // hash
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
  utx.outputs[2] = out;
  s << utx;
  exp =
      "aa000000000000000000000000000000"  // hash
      "00000000000000000000000000000000"  //
      "bb000000"                          // tx height
      "01"                                // isCoinBase
      "01"                                // outputs count
      "02000000"                          // outpoint index
      "cc00000000000000"                  // nValue (-1 by default)
      "016a";                             // scriptPubKey length
  BOOST_CHECK_EQUAL(HexStr(s), exp);
  s.clear();
}

BOOST_AUTO_TEST_CASE(snapshot_p2p_get_snapshot_serialization) {
  snapshot::P2pGetSnapshot msg;
  msg.bestBlockHash.SetHex("bb");
  msg.utxIndex = 55;
  msg.utxCount = 17;

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

  snapshot::P2pGetSnapshot msg2;
  stream >> msg2;
  BOOST_CHECK_EQUAL(msg.bestBlockHash, msg2.bestBlockHash);
  BOOST_CHECK_EQUAL(msg.utxIndex, msg2.utxIndex);
  BOOST_CHECK_EQUAL(msg.utxCount, msg2.utxCount);
}

BOOST_AUTO_TEST_CASE(snapshot_p2p_snapshot_serialization) {
  // serialize empty message
  snapshot::P2pSnapshot msg;
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << msg;
  BOOST_CHECK_EQUAL(stream.size(), 81);

  std::string got = HexStr(stream);
  std::string exp =
      "00000000000000000000000000000000"  // snapshot hash
      "00000000000000000000000000000000"  //
      "00000000000000000000000000000000"  // block hash
      "00000000000000000000000000000000"  //
      "0000000000000000"                  // total utx
      "0000000000000000"                  // index
      "00";                               // utx count
  BOOST_CHECK_EQUAL(got, exp);

  // serialize filled
  msg.snapshotHash.SetHex("aa");
  msg.bestBlockHash.SetHex("bb");
  msg.totalUtxs = 25000000;
  msg.utxIndex = 128;

  snapshot::Utx utx;
  utx.height = 53;
  utx.isCoinBase = true;
  utx.hash.SetHex("bb");
  CScript script;
  script << OP_RETURN;
  utx.outputs[5] = CTxOut(5, script);
  msg.utxs.emplace_back(utx);

  stream.clear();
  stream << msg;
  BOOST_CHECK_EQUAL(stream.size(), 133);

  got = HexStr(stream);
  exp =
      "aa000000000000000000000000000000"  // snapshot hash
      "00000000000000000000000000000000"  //
      "bb000000000000000000000000000000"  // block hash
      "00000000000000000000000000000000"  //
      "40787d0100000000"                  // total utxs
      "8000000000000000"                  // index
      "01"                                // utx count
      "bb000000000000000000000000000000"  // tx hash
      "00000000000000000000000000000000"  //
      "35000000"                          // tx height
      "01"                                // isCoinBase
      "01"                                // output size
      "05000000"                          // output index
      "0500000000000000"                  // amount
      "01"                                // script size
      "6a";                               // script
  BOOST_CHECK_EQUAL(got, exp);

  snapshot::P2pSnapshot msg2;
  stream >> msg2;
  BOOST_CHECK_EQUAL(msg.bestBlockHash, msg2.bestBlockHash);
  BOOST_CHECK_EQUAL(msg.totalUtxs, msg2.totalUtxs);
  BOOST_CHECK_EQUAL(msg.utxIndex, msg2.utxIndex);
  BOOST_CHECK_EQUAL(msg.utxs.size(), msg2.utxs.size());
  BOOST_CHECK_EQUAL(msg.utxs[0].hash, msg2.utxs[0].hash);
  BOOST_CHECK_EQUAL(msg.utxs[0].outputs.size(), msg2.utxs[0].outputs.size());
}

BOOST_AUTO_TEST_SUITE_END()
