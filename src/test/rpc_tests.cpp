// Copyright (c) 2012-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>
#include <rpc/parameter_conversion.h>
#include <rpc/util.h>

#include <core_io.h>
#include <init.h>
#include <interfaces/chain.h>
#include <key_io.h>
#include <netbase.h>

#include <test/test_unite.h>
#include <test/rpc_test_utils.h>

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

#include <univalue.h>

#include <rpc/blockchain.h>


BOOST_FIXTURE_TEST_SUITE(rpc_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(rpc_rawparams)
{
    // Test raw transaction API argument handling
    UniValue r;

    BOOST_CHECK_THROW(CallRPC("getrawtransaction"), std::runtime_error);
    AssertRPCError("getrawtransaction not_hex", RPC_INVALID_PARAMETER);
    BOOST_CHECK_THROW(CallRPC("getrawtransaction a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed not_int"), std::runtime_error);

    BOOST_CHECK_THROW(CallRPC("createrawtransaction"), std::runtime_error);
    AssertRPCError("createrawtransaction null null", RPC_INVALID_PARAMETER);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction not_array"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [] []"));
    AssertRPCError("createrawtransaction {} {}", RPC_TYPE_ERROR);
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [] {}"));
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] {} extra"), std::runtime_error);

    BOOST_CHECK_THROW(CallRPC("decoderawtransaction"), std::runtime_error);
    AssertRPCError("decoderawtransaction null", RPC_DESERIALIZATION_ERROR);
    AssertRPCError("decoderawtransaction DEADBEEF", RPC_DESERIALIZATION_ERROR);
    std::string rawtx = "0100000001a15d57094aa7a21a28cb20b59aab8fc7d1149a3bdbcddba9c622e4f5f6a99ece010000006c493046022100f93bb0e7d8db7bd46e40132d1f8242026e045f03a0efe71bbb8e3f475e970d790221009337cd7f1f929f00cc6ff01f03729b069a7c21b59b1736ddfee5db5946c5da8c0121033b9b137ee87d5a812d6f506efdd37f0affa7ffc310711c06c7f3e097c9447c52ffffffff0100e1f505000000001976a9140389035a9225b3839e2bbf32d826a1e222031fd888ac00000000";
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("decoderawtransaction ")+rawtx));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "size").get_int(), 193);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "version").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "locktime").get_int(), 0);
    BOOST_CHECK_THROW(CallRPC(std::string("decoderawtransaction ")+rawtx+" extra"), std::runtime_error);
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("decoderawtransaction ")+rawtx+" false"));
    BOOST_CHECK_THROW(r = CallRPC(std::string("decoderawtransaction ")+rawtx+" false extra"), std::runtime_error);

    // Only check failure cases for sendrawtransaction, there's no network to send to...
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction"), std::runtime_error);
    AssertRPCError("sendrawtransaction null", RPC_DESERIALIZATION_ERROR);
    AssertRPCError("sendrawtransaction DEADBEEF", RPC_DESERIALIZATION_ERROR);
    BOOST_CHECK_THROW(CallRPC(std::string("sendrawtransaction ")+rawtx+" extra"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_togglenetwork)
{
    UniValue r;

    r = CallRPC("getnetworkinfo");
    bool netState = find_value(r.get_obj(), "networkactive").get_bool();
    BOOST_CHECK_EQUAL(netState, true);

    BOOST_CHECK_NO_THROW(CallRPC("setnetworkactive false"));
    r = CallRPC("getnetworkinfo");
    int numConnection = find_value(r.get_obj(), "connections").get_int();
    BOOST_CHECK_EQUAL(numConnection, 0);

    netState = find_value(r.get_obj(), "networkactive").get_bool();
    BOOST_CHECK_EQUAL(netState, false);

    BOOST_CHECK_NO_THROW(CallRPC("setnetworkactive true"));
    r = CallRPC("getnetworkinfo");
    netState = find_value(r.get_obj(), "networkactive").get_bool();
    BOOST_CHECK_EQUAL(netState, true);
}

BOOST_AUTO_TEST_CASE(rpc_rawsign)
{
    UniValue r;
    // input is a 1-of-2 multisig (so is output):
    std::string prevout =
      "[{\"txid\":\"b4cc287e58f87cdae59417329f710f3ecd75a4ee1d2872b7248f50977c8493f3\","
      "\"vout\":1,\"scriptPubKey\":\"a914b10c9df5f7edf436c697f02f1efdba4cf399615187\","
      "\"redeemScript\":\"512103debedc17b3df2badbcdd86d5feb4562b86fe182e5998abd8bcd4f122c6155b1b21027e940bb73ab8732bfdf7f9216ecefca5b94d6df834e77e108f68e66f126044c052ae\"}]";
    r = CallRPC(std::string("createrawtransaction ")+prevout+" "+
      "{\"" "2N9PNhtGuyeF95Sybt7AT8QnBQvgiFWgEyV" "\":11}");
    std::string notsigned = r.get_str();
    std::string privkey1 = "\"" "cREXSWp1AanKYX81uR8cKjuAmY1HpN6CBNxKoMgbapjrkuEooecf" "\"";
    std::string privkey2 = "\"" "cQ4d7zLkkX8DE87MYPQHZCDXnk2u5mADLzJnrShuqvgHz1QTEXkV" "\"";
    InitInterfaces interfaces;
    interfaces.chain = interfaces::MakeChain();
    g_rpc_interfaces = &interfaces;
    r = CallRPC(std::string("signrawtransactionwithkey ")+notsigned+" [] "+prevout);
    BOOST_CHECK(find_value(r.get_obj(), "complete").get_bool() == false);
    r = CallRPC(std::string("signrawtransactionwithkey ")+notsigned+" ["+privkey1+","+privkey2+"] "+prevout);
    BOOST_CHECK(find_value(r.get_obj(), "complete").get_bool() == true);
    g_rpc_interfaces = nullptr;
}

BOOST_AUTO_TEST_CASE(rpc_createraw_op_return)
{
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [{\"txid\":\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed\",\"vout\":0}] {\"data\":\"68656c6c6f776f726c64\"}"));

    // Key not "data" (bad address)
    AssertRPCError("createrawtransaction [{\"txid\":\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed\",\"vout\":0}] {\"somedata\":\"68656c6c6f776f726c64\"}", RPC_INVALID_ADDRESS_OR_KEY);

    // Bad hex encoding of data output
    AssertRPCError("createrawtransaction [{\"txid\":\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed\",\"vout\":0}] {\"data\":\"12345\"}", RPC_INVALID_PARAMETER);
    AssertRPCError("createrawtransaction [{\"txid\":\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed\",\"vout\":0}] {\"data\":\"12345g\"}", RPC_INVALID_PARAMETER);

    // Data 81 bytes long
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [{\"txid\":\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed\",\"vout\":0}] {\"data\":\"010203040506070809101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960616263646566676869707172737475767778798081\"}"));
}

BOOST_AUTO_TEST_CASE(rpc_format_monetary_values)
{
    BOOST_CHECK(ValueFromAmount(0LL).write() == "0.00000000");
    BOOST_CHECK(ValueFromAmount(1LL).write() == "0.00000001");
    BOOST_CHECK(ValueFromAmount(17622195LL).write() == "0.17622195");
    BOOST_CHECK(ValueFromAmount(50000000LL).write() == "0.50000000");
    BOOST_CHECK(ValueFromAmount(89898989LL).write() == "0.89898989");
    BOOST_CHECK(ValueFromAmount(100000000LL).write() == "1.00000000");
    BOOST_CHECK(ValueFromAmount(2099999999999990LL).write() == "20999999.99999990");
    BOOST_CHECK(ValueFromAmount(2099999999999999LL).write() == "20999999.99999999");

    BOOST_CHECK_EQUAL(ValueFromAmount(0).write(), "0.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount((UNIT/10000)*123456789).write(), "12345.67890000");
    BOOST_CHECK_EQUAL(ValueFromAmount(-UNIT).write(), "-1.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(-UNIT/10).write(), "-0.10000000");

    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT*100000000).write(), "100000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT*10000000).write(), "10000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT*1000000).write(), "1000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT*100000).write(), "100000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT*10000).write(), "10000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT*1000).write(), "1000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT*100).write(), "100.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT*10).write(), "10.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT).write(), "1.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT/10).write(), "0.10000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT/100).write(), "0.01000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT/1000).write(), "0.00100000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT/10000).write(), "0.00010000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT/100000).write(), "0.00001000");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT/1000000).write(), "0.00000100");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT/10000000).write(), "0.00000010");
    BOOST_CHECK_EQUAL(ValueFromAmount(UNIT/100000000).write(), "0.00000001");
}

static UniValue ValueFromString(const std::string &str)
{
    UniValue value;
    BOOST_CHECK(value.setNumStr(str));
    return value;
}

BOOST_AUTO_TEST_CASE(rpc_parse_monetary_values)
{
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("-0.00000001")), UniValue);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0")), 0LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.00000000")), 0LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.00000001")), 1LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.17622195")), 17622195LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.5")), 50000000LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.50000000")), 50000000LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.89898989")), 89898989LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("1.00000000")), 100000000LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("20999999.9999999")), 2099999999999990LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("20999999.99999999")), 2099999999999999LL);

    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("1e-8")), UNIT/100000000);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.1e-7")), UNIT/100000000);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.01e-6")), UNIT/100000000);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.0000000000000000000000000000000000000000000000000000000000000000000000000001e+68")), UNIT/100000000);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("10000000000000000000000000000000000000000000000000000000000000000e-64")), UNIT);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000e64")), UNIT);

    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("1e-9")), UniValue); //should fail
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("0.000000019")), UniValue); //should fail
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.00000001000000")), 1LL); //should pass, cut trailing 0
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("19e-9")), UniValue); //should fail
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.19e-6")), 19); //should pass, leading 0 is present

    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("92233720368.54775808")), UniValue); //overflow error
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("1e+11")), UniValue); //overflow error
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("1e11")), UniValue); //overflow error signless
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("93e+9")), UniValue); //overflow error
}

BOOST_AUTO_TEST_CASE(json_parse_errors)
{
    // Valid
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue("1.0").get_real(), 1.0);
    // Valid, with leading or trailing whitespace
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue(" 1.0").get_real(), 1.0);
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue("1.0 ").get_real(), 1.0);

    BOOST_CHECK_THROW(AmountFromValue(ParseNonRFCJSONValue(".19e-6")), std::runtime_error); //should fail, missing leading 0, therefore invalid JSON
    BOOST_CHECK_EQUAL(AmountFromValue(ParseNonRFCJSONValue("0.00000000000000000000000000000000000001e+30 ")), 1);
    // Invalid, initial garbage
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("[1.0"), std::runtime_error);
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("a1.0"), std::runtime_error);
    // Invalid, trailing garbage
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("1.0sds"), std::runtime_error);
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("1.0]"), std::runtime_error);
    // UTE addresses should fail parsing
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"), std::runtime_error);
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("3J98t1WpEZ73CNmQviecrnyiWrnqRhWNL"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_ban)
{
    BOOST_CHECK_NO_THROW(CallRPC("clearbanned"));

    UniValue r;
    BOOST_CHECK_NO_THROW(r = CallRPC("setban 127.0.0.0 add"));
    BOOST_CHECK_THROW(r = CallRPC("setban 127.0.0.0:8334"), std::runtime_error); //portnumber for setban not allowed
    BOOST_CHECK_NO_THROW(r = CallRPC("listbanned"));
    UniValue ar = r.get_array();
    UniValue o1 = ar[0].get_obj();
    UniValue adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/32");
    BOOST_CHECK_NO_THROW(CallRPC("setban 127.0.0.0 remove"));
    BOOST_CHECK_NO_THROW(r = CallRPC("listbanned"));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0U);

    BOOST_CHECK_NO_THROW(r = CallRPC("setban 127.0.0.0/24 add 9907731200 true"));
    BOOST_CHECK_NO_THROW(r = CallRPC("listbanned"));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    UniValue banned_until = find_value(o1, "banned_until");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/24");
    BOOST_CHECK_EQUAL(banned_until.get_int64(), 9907731200); // absolute time check

    BOOST_CHECK_NO_THROW(CallRPC("clearbanned"));

    BOOST_CHECK_NO_THROW(r = CallRPC("setban 127.0.0.0/24 add 200"));
    BOOST_CHECK_NO_THROW(r = CallRPC("listbanned"));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    banned_until = find_value(o1, "banned_until");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/24");
    int64_t now = GetTime();
    BOOST_CHECK(banned_until.get_int64() > now);
    BOOST_CHECK(banned_until.get_int64()-now <= 200);

    // must throw an exception because 127.0.0.1 is in already banned subnet range
    AssertRPCError("setban 127.0.0.1 add", RPC_CLIENT_NODE_ALREADY_ADDED);

    BOOST_CHECK_NO_THROW(CallRPC("setban 127.0.0.0/24 remove"));
    BOOST_CHECK_NO_THROW(r = CallRPC("listbanned"));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0U);

    BOOST_CHECK_NO_THROW(r = CallRPC("setban 127.0.0.0/255.255.0.0 add"));
    AssertRPCError("setban 127.0.1.1 add", RPC_CLIENT_NODE_ALREADY_ADDED);

    BOOST_CHECK_NO_THROW(CallRPC("clearbanned"));
    BOOST_CHECK_NO_THROW(r = CallRPC("listbanned"));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0U);


    AssertRPCError("setban test add", RPC_CLIENT_INVALID_IP_OR_SUBNET); //invalid IP

    //IPv6 tests
    BOOST_CHECK_NO_THROW(r = CallRPC("setban FE80:0000:0000:0000:0202:B3FF:FE1E:8329 add"));
    BOOST_CHECK_NO_THROW(r = CallRPC("listbanned"));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "fe80::202:b3ff:fe1e:8329/128");

    BOOST_CHECK_NO_THROW(CallRPC("clearbanned"));
    BOOST_CHECK_NO_THROW(r = CallRPC("setban 2001:db8::/ffff:fffc:0:0:0:0:0:0 add"));
    BOOST_CHECK_NO_THROW(r = CallRPC("listbanned"));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "2001:db8::/30");

    BOOST_CHECK_NO_THROW(CallRPC("clearbanned"));
    BOOST_CHECK_NO_THROW(r = CallRPC("setban 2001:4d48:ac57:400:cacf:e9ff:fe1d:9c63/128 add"));
    BOOST_CHECK_NO_THROW(r = CallRPC("listbanned"));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "2001:4d48:ac57:400:cacf:e9ff:fe1d:9c63/128");
}

BOOST_AUTO_TEST_CASE(rpc_convert_values_generatetoaddress)
{
    UniValue result;

    BOOST_CHECK_NO_THROW(result = RPCConvertValues("generatetoaddress", {"101", "mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a"}));
    BOOST_CHECK_EQUAL(result[0].get_int(), 101);
    BOOST_CHECK_EQUAL(result[1].get_str(), "mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a");

    BOOST_CHECK_NO_THROW(result = RPCConvertValues("generatetoaddress", {"101", "mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU"}));
    BOOST_CHECK_EQUAL(result[0].get_int(), 101);
    BOOST_CHECK_EQUAL(result[1].get_str(), "mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU");

    BOOST_CHECK_NO_THROW(result = RPCConvertValues("generatetoaddress", {"1", "mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a", "9"}));
    BOOST_CHECK_EQUAL(result[0].get_int(), 1);
    BOOST_CHECK_EQUAL(result[1].get_str(), "mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a");
    BOOST_CHECK_EQUAL(result[2].get_int(), 9);

    BOOST_CHECK_NO_THROW(result = RPCConvertValues("generatetoaddress", {"1", "mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU", "9"}));
    BOOST_CHECK_EQUAL(result[0].get_int(), 1);
    BOOST_CHECK_EQUAL(result[1].get_str(), "mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU");
    BOOST_CHECK_EQUAL(result[2].get_int(), 9);
}

BOOST_AUTO_TEST_CASE(rpc_getblockstats_calculate_percentiles_by_weight)
{
    int64_t total_weight = 200;
    std::vector<std::pair<CAmount, int64_t>> feerates;
    CAmount result[NUM_GETBLOCKSTATS_PERCENTILES] = { 0 };

    for (int64_t i = 0; i < 100; i++) {
        feerates.emplace_back(std::make_pair(1 ,1));
    }

    for (int64_t i = 0; i < 100; i++) {
        feerates.emplace_back(std::make_pair(2 ,1));
    }

    CalculatePercentilesByWeight(result, feerates, total_weight);
    BOOST_CHECK_EQUAL(result[0], 1);
    BOOST_CHECK_EQUAL(result[1], 1);
    BOOST_CHECK_EQUAL(result[2], 1);
    BOOST_CHECK_EQUAL(result[3], 2);
    BOOST_CHECK_EQUAL(result[4], 2);

    // Test with more pairs, and two pairs overlapping 2 percentiles.
    total_weight = 100;
    CAmount result2[NUM_GETBLOCKSTATS_PERCENTILES] = { 0 };
    feerates.clear();

    feerates.emplace_back(std::make_pair(1, 9));
    feerates.emplace_back(std::make_pair(2 , 16)); //10th + 25th percentile
    feerates.emplace_back(std::make_pair(4 ,50)); //50th + 75th percentile
    feerates.emplace_back(std::make_pair(5 ,10));
    feerates.emplace_back(std::make_pair(9 ,15));  // 90th percentile

    CalculatePercentilesByWeight(result2, feerates, total_weight);

    BOOST_CHECK_EQUAL(result2[0], 2);
    BOOST_CHECK_EQUAL(result2[1], 2);
    BOOST_CHECK_EQUAL(result2[2], 4);
    BOOST_CHECK_EQUAL(result2[3], 4);
    BOOST_CHECK_EQUAL(result2[4], 9);

    // Same test as above, but one of the percentile-overlapping pairs is split in 2.
    total_weight = 100;
    CAmount result3[NUM_GETBLOCKSTATS_PERCENTILES] = { 0 };
    feerates.clear();

    feerates.emplace_back(std::make_pair(1, 9));
    feerates.emplace_back(std::make_pair(2 , 11)); // 10th percentile
    feerates.emplace_back(std::make_pair(2 , 5)); // 25th percentile
    feerates.emplace_back(std::make_pair(4 ,50)); //50th + 75th percentile
    feerates.emplace_back(std::make_pair(5 ,10));
    feerates.emplace_back(std::make_pair(9 ,15)); // 90th percentile

    CalculatePercentilesByWeight(result3, feerates, total_weight);

    BOOST_CHECK_EQUAL(result3[0], 2);
    BOOST_CHECK_EQUAL(result3[1], 2);
    BOOST_CHECK_EQUAL(result3[2], 4);
    BOOST_CHECK_EQUAL(result3[3], 4);
    BOOST_CHECK_EQUAL(result3[4], 9);

    // Test with one transaction spanning all percentiles.
    total_weight = 104;
    CAmount result4[NUM_GETBLOCKSTATS_PERCENTILES] = { 0 };
    feerates.clear();

    feerates.emplace_back(std::make_pair(1, 100));
    feerates.emplace_back(std::make_pair(2, 1));
    feerates.emplace_back(std::make_pair(3, 1));
    feerates.emplace_back(std::make_pair(3, 1));
    feerates.emplace_back(std::make_pair(999999, 1));

    CalculatePercentilesByWeight(result4, feerates, total_weight);

    for (int64_t i = 0; i < NUM_GETBLOCKSTATS_PERCENTILES; i++) {
        BOOST_CHECK_EQUAL(result4[i], 1);
    }
}

BOOST_AUTO_TEST_SUITE_END()
