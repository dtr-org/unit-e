// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/messages.h>

#include <test/test_unite.h>
#include <utilstrencodings.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_messages_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(utxo_subset_serializer) {
  CDataStream s(SER_NETWORK, INIT_PROTO_VERSION);
  auto subset = snapshot::UTXOSubset();
  s << subset;

  std::string exp;
  exp =
      "00000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "00000000"                          // height
      "00"                                // tx_type
      "00";                               // outputs
  BOOST_CHECK_EQUAL(HexStr(s), exp);
  s.clear();

  subset.tx_id.SetHex("aa");
  subset.height = 0xbb;
  subset.tx_type = TxType::COINBASE;
  s << subset;
  exp =
      "aa000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "bb000000"                          // tx height
      "01"                                // tx_type
      "00";                               // outputs
  BOOST_CHECK_MESSAGE(HexStr(s) == exp,
                      "expected: " << HexStr(s) << " got: " << exp);
  s.clear();

  subset.outputs[2] = CTxOut();
  s << subset;
  exp =
      "aa000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "bb000000"                          // tx height
      "01"                                // tx_type
      "01"                                // outputs count
      "02000000"                          // outpoint index
      "ffffffffffffffff"                  // nValue (-1 by default)
      "00";                               // scriptPubKey length
  BOOST_CHECK_EQUAL(HexStr(s), exp);
  s.clear();

  auto out = CTxOut();
  out.nValue = 0xcc;
  out.scriptPubKey << OP_RETURN;
  subset.outputs[2] = out;
  s << subset;
  exp =
      "aa000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "bb000000"                          // tx height
      "01"                                // tx_type
      "01"                                // outputs count
      "02000000"                          // outpoint index
      "cc00000000000000"                  // nValue (-1 by default)
      "01"                                // scriptPubKey length
      "6a";                               // script data
  BOOST_CHECK_EQUAL(HexStr(s), exp);
  s.clear();
}

BOOST_AUTO_TEST_CASE(snapshot_header_serialization) {
  snapshot::SnapshotHeader msg;
  msg.snapshot_hash.SetHex("aa");
  msg.block_hash.SetHex("bb");
  msg.stake_modifier.SetHex("cc");
  msg.chain_work.SetHex("dd");
  msg.total_utxo_subsets = 10;

  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << msg;
  BOOST_CHECK_EQUAL(stream.size(), 136);

  std::string exp =
      "aa000000000000000000000000000000"  // snapshot_hash
      "00000000000000000000000000000000"  //
      "bb000000000000000000000000000000"  // block_hash
      "00000000000000000000000000000000"  //
      "cc000000000000000000000000000000"  // stake_modifier
      "00000000000000000000000000000000"  //
      "dd000000000000000000000000000000"  // chain_work
      "00000000000000000000000000000000"  //
      "0a00000000000000";                 // total_utxo_subsets
  BOOST_CHECK_EQUAL(HexStr(stream), exp);

  snapshot::SnapshotHeader msg2;
  stream >> msg2;
  BOOST_CHECK_EQUAL(msg.snapshot_hash, msg2.snapshot_hash);
  BOOST_CHECK_EQUAL(msg.block_hash, msg2.block_hash);
  BOOST_CHECK_EQUAL(msg.stake_modifier, msg2.stake_modifier);
  BOOST_CHECK_EQUAL(msg.chain_work, msg2.chain_work);
  BOOST_CHECK_EQUAL(msg.total_utxo_subsets, msg2.total_utxo_subsets);
}

BOOST_AUTO_TEST_CASE(snapshot_header_comparison) {
  snapshot::SnapshotHeader a;
  snapshot::SnapshotHeader b;
  BOOST_CHECK(a.IsNull());
  BOOST_CHECK(b.IsNull());
  BOOST_CHECK(a == b);

  a.snapshot_hash.SetHex("aa");
  b.snapshot_hash.SetHex("aa");
  BOOST_CHECK(!a.IsNull());
  BOOST_CHECK(!b.IsNull());
  BOOST_CHECK(a == b);

  b.snapshot_hash.SetHex("bb");
  BOOST_CHECK(a != b);

  a.SetNull();
  BOOST_CHECK(a.IsNull());
  BOOST_CHECK(a != b);
}

BOOST_AUTO_TEST_CASE(get_snapshot_serialization) {
  snapshot::GetSnapshot msg;
  msg.snapshot_hash.SetHex("bb");
  msg.utxo_subset_index = 55;
  msg.utxo_subset_count = 17;

  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << msg;
  BOOST_CHECK_MESSAGE(stream.size() == 42,
                      "size expected: " << 42 << " got: " << stream.size());

  std::string got = HexStr(stream);
  std::string exp =
      "bb000000000000000000000000000000"  // snapshot_hash
      "00000000000000000000000000000000"  //
      "3700000000000000"                  // index
      "1100";                             // length
  BOOST_CHECK_MESSAGE(got == exp, "expected: " << exp << " got: " << got);

  snapshot::GetSnapshot msg2;
  stream >> msg2;
  BOOST_CHECK_EQUAL(msg.snapshot_hash, msg2.snapshot_hash);
  BOOST_CHECK_EQUAL(msg.utxo_subset_index, msg2.utxo_subset_index);
  BOOST_CHECK_EQUAL(msg.utxo_subset_count, msg2.utxo_subset_count);
}

BOOST_AUTO_TEST_CASE(snapshot_serialization) {
  // serialize empty message
  snapshot::Snapshot msg;
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << msg;
  BOOST_CHECK_EQUAL(stream.size(), 41);

  std::string got = HexStr(stream);
  std::string exp =
      "00000000000000000000000000000000"  // snapshot hash
      "00000000000000000000000000000000"  //
      "0000000000000000"                  // utxo set index
      "00";                               // utxo set count
  BOOST_CHECK_EQUAL(got, exp);

  // serialize filled
  msg.snapshot_hash.SetHex("aa");
  msg.utxo_subset_index = 128;

  snapshot::UTXOSubset subset;
  subset.height = 53;
  subset.tx_type = TxType::COINBASE;
  subset.tx_id.SetHex("bb");
  CScript script;
  script << OP_RETURN;
  subset.outputs[5] = CTxOut(5, script);
  msg.utxo_subsets.emplace_back(subset);

  stream.clear();
  stream << msg;
  BOOST_CHECK_EQUAL(stream.size(), 93);

  got = HexStr(stream);
  exp =
      "aa000000000000000000000000000000"  // snapshot hash
      "00000000000000000000000000000000"  //
      "8000000000000000"                  // index
      "01"                                // utxo set count
      "bb000000000000000000000000000000"  // tx id
      "00000000000000000000000000000000"  //
      "35000000"                          // tx height
      "01"                                // tx_type
      "01"                                // output size
      "05000000"                          // output index
      "0500000000000000"                  // amount
      "01"                                // script size
      "6a";                               // script
  BOOST_CHECK_EQUAL(got, exp);

  snapshot::Snapshot msg2;
  stream >> msg2;
  BOOST_CHECK_EQUAL(msg.utxo_subset_index, msg2.utxo_subset_index);
  BOOST_CHECK_EQUAL(msg.utxo_subsets.size(), msg2.utxo_subsets.size());
  BOOST_CHECK_EQUAL(msg.utxo_subsets[0].tx_id, msg2.utxo_subsets[0].tx_id);
  BOOST_CHECK_EQUAL(msg.utxo_subsets[0].outputs.size(),
                    msg2.utxo_subsets[0].outputs.size());
}

BOOST_AUTO_TEST_CASE(utxo_construct) {
  COutPoint out;
  Coin coin;
  snapshot::UTXO utxo1(out, coin);
  BOOST_CHECK_EQUAL(utxo1.out_point.hash, out.hash);
  BOOST_CHECK_EQUAL(utxo1.out_point.n, out.n);
  BOOST_CHECK_EQUAL(utxo1.height, coin.nHeight);
  BOOST_CHECK_EQUAL(utxo1.tx_type, coin.tx_type);
  BOOST_CHECK(utxo1.tx_out == coin.out);

  out.hash.SetHex("aa");
  out.n = 10;
  coin.nHeight = 250;
  coin.tx_type = TxType::COINBASE;
  coin.out.nValue = 35;
  coin.out.scriptPubKey << OP_RETURN;

  snapshot::UTXO utxo2(out, coin);
  BOOST_CHECK_EQUAL(utxo2.out_point.hash, out.hash);
  BOOST_CHECK_EQUAL(utxo2.out_point.n, out.n);
  BOOST_CHECK_EQUAL(utxo2.height, coin.nHeight);
  BOOST_CHECK_EQUAL(utxo2.tx_type, coin.tx_type);
  BOOST_CHECK(utxo2.tx_out == coin.out);
}

BOOST_AUTO_TEST_CASE(utxo_serialization) {
  snapshot::UTXO utxo1;
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << utxo1;
  BOOST_CHECK_EQUAL(stream.size(), 50);

  std::string got = HexStr(stream);
  std::string exp =
      "00000000000000000000000000000000"  // txId
      "00000000000000000000000000000000"  //
      "ffffffff"                          // txOutIndex
      "00000000"                          // height
      "00"                                // tx_type
      "ffffffffffffffff"                  // CAmount(-1)
      "00";                               // script length
  BOOST_CHECK_EQUAL(got, exp);
  stream.clear();

  COutPoint out;
  out.hash.SetHex("aa");
  out.n = 10;
  Coin coin;
  coin.nHeight = 250;
  coin.tx_type = TxType::COINBASE;
  coin.out.nValue = 35;
  coin.out.scriptPubKey << OP_RETURN;

  snapshot::UTXO utxo2(out, coin);
  stream << utxo2;
  BOOST_CHECK_EQUAL(stream.size(), 51);

  got = HexStr(stream);
  exp =
      "aa000000000000000000000000000000"  // txId
      "00000000000000000000000000000000"  //
      "0a000000"                          // txOutIndex
      "fa000000"                          // height
      "01"                                // tx_type
      "2300000000000000"                  // CAmount
      "01"                                // script length
      "6a";                               // script
  BOOST_CHECK_EQUAL(got, exp);
  stream.clear();
}

BOOST_AUTO_TEST_CASE(snapshot_hash) {
  // expected results are hardcoded to guarantee that hashes
  // didn't change over time
  uint256 stake_modifier;
  uint256 chain_work;
  snapshot::SnapshotHash h;
  std::string null_hash = h.GetHash(stake_modifier, chain_work).GetHex();

  snapshot::UTXO a;
  a.out_point.hash.SetHex("aa");
  snapshot::UTXO b;
  b.out_point.hash.SetHex("bb");
  snapshot::UTXO c;
  c.out_point.hash.SetHex("cc");

  std::string a_hash =
      "c26e897fa6ee45709c6d91e02e603082"
      "76fe3bf8d4cf2c6032e9a747c31c9852";
  std::string b_hash =
      "4d471506479905d1044fb83eba35ee55"
      "a9cd5b700b089e5054cdf7e22a03f2bd";
  std::string ab_sum_hash =
      "ab12fb63b1ccb7a4a6cfe487a9ad5273"
      "1ac0daae50c93726930caddcbe0db281";

  {
    // hashing twice produces the same result
    snapshot::SnapshotHash hash;
    hash.AddUTXO(a);
    hash.AddUTXO(b);
    BOOST_CHECK_EQUAL(hash.GetHash(stake_modifier, chain_work).GetHex(), ab_sum_hash);
    BOOST_CHECK_EQUAL(hash.GetHash(stake_modifier, chain_work).GetHex(), ab_sum_hash);

    // changing stake_modifier produces different hash
    uint256 new_sm;
    new_sm.SetHex("bb");
    BOOST_CHECK(hash.GetHash(new_sm, chain_work).GetHex() != ab_sum_hash);

    // changing chain_work produces different hash
    uint256 new_cw;
    new_cw.SetHex("cc");
    BOOST_CHECK(hash.GetHash(stake_modifier, new_cw).GetHex() != ab_sum_hash);

    // changing stake_modifier or chain_work doesn't change underline UTXO data
    BOOST_CHECK_EQUAL(hash.GetHash(stake_modifier, chain_work).GetHex(), ab_sum_hash);
  }

  {
    // test adding and reverting UTXOs
    // null == a + b - b - a
    snapshot::SnapshotHash hash;
    BOOST_CHECK_EQUAL(hash.GetHash(stake_modifier, chain_work).GetHex(), null_hash);
    hash.AddUTXO(a);
    BOOST_CHECK_EQUAL(hash.GetHash(stake_modifier, chain_work).GetHex(), a_hash);
    hash.AddUTXO(b);
    BOOST_CHECK_EQUAL(hash.GetHash(stake_modifier, chain_work).GetHex(), ab_sum_hash);
    hash.SubtractUTXO(b);
    BOOST_CHECK_EQUAL(hash.GetHash(stake_modifier, chain_work).GetHex(), a_hash);
    hash.SubtractUTXO(a);
    BOOST_CHECK_EQUAL(hash.GetHash(stake_modifier, chain_work).GetHex(), null_hash);
  }

  {
    // test that order doesn't matter
    // a + b == b + a
    snapshot::SnapshotHash hash1;
    snapshot::SnapshotHash hash2;
    hash1.AddUTXO(a);
    hash1.AddUTXO(b);
    hash2.AddUTXO(b);
    hash2.AddUTXO(a);
    BOOST_CHECK_EQUAL(hash1.GetHash(stake_modifier, chain_work).GetHex(), ab_sum_hash);
    BOOST_CHECK_EQUAL(hash2.GetHash(stake_modifier, chain_work).GetHex(), ab_sum_hash);
  }

  {
    // that subtraction
    // b = a + b + c - a - c
    snapshot::SnapshotHash hash1;
    hash1.AddUTXO(a);
    hash1.AddUTXO(b);
    hash1.AddUTXO(c);
    hash1.SubtractUTXO(a);
    hash1.SubtractUTXO(c);

    snapshot::SnapshotHash hash2;
    hash2.AddUTXO(b);

    BOOST_CHECK_EQUAL(hash1.GetHash(stake_modifier, chain_work).GetHex(), b_hash);
    BOOST_CHECK_EQUAL(hash2.GetHash(stake_modifier, chain_work).GetHex(), b_hash);
  }

  {
    // negative case
    // null = -a + a
    // a = -a + a + a
    snapshot::SnapshotHash hash;
    hash.SubtractUTXO(a);
    BOOST_CHECK(hash.GetHash(stake_modifier, chain_work).GetHex() != null_hash);
    hash.AddUTXO(a);
    BOOST_CHECK_EQUAL(hash.GetHash(stake_modifier, chain_work).GetHex(), null_hash);
    hash.AddUTXO(a);
    BOOST_CHECK_EQUAL(hash.GetHash(stake_modifier, chain_work).GetHex(), a_hash);
  }

  {
    // restore snapshotHash from disk

    snapshot::SnapshotHash hash1;
    hash1.AddUTXO(a);
    hash1.AddUTXO(b);

    // simulate reading snapshot data from disk
    snapshot::SnapshotHash hash2(hash1.GetData());

    BOOST_CHECK_EQUAL(hash1.GetHash(stake_modifier, chain_work).GetHex(),
                      hash2.GetHash(stake_modifier, chain_work).GetHex());
    hash1.AddUTXO(c);
    hash1.SubtractUTXO(a);
    hash2.AddUTXO(c);
    hash2.SubtractUTXO(a);
    BOOST_CHECK_EQUAL(hash1.GetHash(stake_modifier, chain_work).GetHex(),
                      hash2.GetHash(stake_modifier, chain_work).GetHex());
  }
}

BOOST_AUTO_TEST_SUITE_END()
