// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/parameter_conversion.h>
#include <rpc/server.h>
#include <test/rpc_test_utils.h>

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

UniValue CallRPC(std::string args) {

  std::vector<std::string> vArgs;
  boost::split(vArgs, args, boost::is_any_of(" \t"));

  std::string strMethod = vArgs[0];
  vArgs.erase(vArgs.begin());

  JSONRPCRequest request;
  request.strMethod = strMethod;
  request.params = RPCConvertValues(strMethod, vArgs);
  request.fHelp = false;

  BOOST_CHECK(tableRPC[strMethod]);
  rpcfn_type method = tableRPC[strMethod]->actor;

  try {
    UniValue result = (*method)(request);
    return result;
  } catch (const UniValue &objError) {
    throw RPCErrorResult{
        static_cast<RPCErrorCode>(find_value(objError, "code").get_int()),
        find_value(objError, "message").get_str()};
  }
}

void AssertRPCError(std::string call, RPCErrorCode error, const std::string &message) {
  try {
    CallRPC(std::move(call));
    assert(false); // an exception should be thrown before this point.
  } catch (const RPCErrorResult &err) {
    if (!message.empty()) {
      BOOST_CHECK_EQUAL(err.message, message);
    }
    BOOST_CHECK_EQUAL(err.errorCode, error);
  }
}
