#ifndef UNIT_E_TEST_RPC_TEST_UTILS_H
#define UNIT_E_TEST_RPC_TEST_UTILS_H

#include <rpc/protocol.h>
#include <univalue.h>

struct RPCErrorResult {
  RPCErrorCode errorCode;
  std::string message;
};

//! Calls the rpc interface with the given string.
//! \param args the string composed by command name and params.
//! \return the unserialized result.
UniValue CallRPC(std::string args);

void AssertRPCError(std::string call, RPCErrorCode error);

#endif  // UNIT_E_TEST_RPC_TEST_UTILS_H